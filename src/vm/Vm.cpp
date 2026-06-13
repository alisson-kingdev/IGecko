#include "Vm.h"
#include "../compiler/Compiler.h"
#include "Disassembler.h"
#include "Object.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "../lexer/Lexer.h"
#include "../parser/Parser.h"

// --- Module resolution helpers ---

std::string Vm::readFile(const std::string& path)
{
  std::ifstream file(path);
  if (!file.is_open()) {
    return "";
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

std::string Vm::resolveModulePath(const std::string& path, const std::string& currentFile)
{
  namespace fs = std::filesystem;

  // Check if it's a stdlib module (no path separators, no extension)
  if (path.find('/') == std::string::npos && path.find('\\') == std::string::npos
      && path.find('.') == std::string::npos) {
    // Try project stdlib path
    std::string projectStdlib = fs::absolute("src/stdlib/" + path + "/index.gk").string();
    if (fs::exists(projectStdlib)) return projectStdlib;

    // Try default stdlib path
    std::string defaultStdlib = std::string("/opt/gecko/stdlibs/") + path + "/index.gk";
    if (fs::exists(defaultStdlib)) return defaultStdlib;
  }

  // Local file relative to the importing file
  fs::path base = fs::path(currentFile).parent_path();
  fs::path resolved = fs::absolute(base / path);

  // If the path already has .gk extension, use it directly
  if (resolved.extension() == ".gk") {
    if (fs::exists(resolved)) return resolved.string();
    return "";
  }

  // Try with .gk extension
  fs::path withExt = resolved;
  withExt.replace_extension(".gk");
  if (fs::exists(withExt)) return withExt.string();

  // Try as directory with index.gk
  fs::path asDir = resolved / "index.gk";
  if (fs::exists(asDir)) return asDir.string();

  return "";
}

InterpretResult Vm::resolveAndExecuteImport(const std::string& path, const std::string& currentFile,
                                            std::map<std::string, Value>& exports)
{
  std::string resolvedPath = resolveModulePath(path, currentFile);
  if (resolvedPath.empty()) {
    return { InterpretResult::COMPILE_ERROR, "Module not found: " + path };
  }

  // Check if already loaded
  std::string loadKey = "__loaded__" + resolvedPath;
  auto loadedIt = globals.find(loadKey);
  if (loadedIt != globals.end()) {
    // Restore exports from saved state
    std::string exportKey = "__exports__" + resolvedPath;
    auto expIt = globals.find(exportKey);
    if (expIt != globals.end() && expIt->second.isObj()
        && expIt->second.asObj()->type == ObjType::INSTANCE) {
      auto* obj = static_cast<ObjInstance*>(expIt->second.asObj());
      for (auto& [k, v] : obj->fields) {
        exports[k] = v;
      }
    }
    return { InterpretResult::OK, "" };
  }

  std::string source = readFile(resolvedPath);
  if (source.empty()) {
    return { InterpretResult::COMPILE_ERROR, "Cannot read module: " + resolvedPath };
  }

  // Lex and parse the module to find its own imports and exports
  Lexer lexer(source, resolvedPath);
  auto tokens = lexer.tokenize();
  Parser parser(tokens);
  auto statements = parser.parse();

  // Find export names from the AST
  std::vector<std::string> exportNames;
  bool hasPackage = false;
  for (auto& stmt : statements) {
    if (auto* exportStmt = dynamic_cast<ExportStmt*>(stmt.get())) {
      for (auto& nameTok : exportStmt->names) {
        exportNames.push_back(nameTok.lexeme);
      }
    }
    if (dynamic_cast<PackageStmt*>(stmt.get()) != nullptr) {
      hasPackage = true;
    }
  }
  // For package-style modules without explicit exports, collect all top-level names
  if (exportNames.empty() && hasPackage) {
    for (auto& stmt : statements) {
      if (auto* funcStmt = dynamic_cast<FunctionStmt*>(stmt.get())) {
        exportNames.push_back(funcStmt->name.lexeme);
      } else if (auto* varStmt = dynamic_cast<VarStmt*>(stmt.get())) {
        exportNames.push_back(varStmt->name.lexeme);
      }
    }
  }

  // Recursively resolve the module's imports first
  for (auto& stmt : statements) {
    if (auto* importStmt = dynamic_cast<ImportStmt*>(stmt.get())) {
      std::map<std::string, Value> subExports;
      InterpretResult r = resolveAndExecuteImport(importStmt->modulePath, resolvedPath, subExports);
      if (r.status != InterpretResult::OK) return r;
      // Handle sub-imports in the module's scope
      if (importStmt->isNamespaceImport && !importStmt->names.empty()) {
        auto* obj = new ObjInstance(nullptr);
        objects.push_back(obj);
        for (auto& [k, v] : subExports) obj->fields[k] = v;
        globals[importStmt->names[0].lexeme] = OBJ_VAL(obj);
      } else if (importStmt->importAll) {
        for (auto& [k, v] : subExports) globals[k] = v;
      } else {
        for (auto& n : importStmt->names) {
          auto it = subExports.find(n.lexeme);
          if (it != subExports.end()) globals[n.lexeme] = it->second;
        }
      }
    }
  }


  // Compile and execute the module
  Compiler compiler;
  ObjFunction* fn = compiler.compileAST(statements);
  if (fn == nullptr) {
    return { InterpretResult::COMPILE_ERROR, "Failed to compile module: " + resolvedPath };
  }


  objects.insert(objects.end(), compiler.objects.begin(), compiler.objects.end());

  auto* closure = new ObjClosure(fn);
  objects.push_back(closure);

  push(OBJ_VAL(closure));
  frameCount = 0;
  CallFrame* frame = &frames[frameCount++];
  frame->closure = closure;
  frame->ip = fn->chunk.bytecode.data();
  frame->slots = stackTop - 1;

  InterpretResult result = run();
  if (result.status != InterpretResult::OK) {
    return result;
  }

  // Collect exports from globals
  for (auto& name : exportNames) {
    auto it = globals.find(name);
    if (it != globals.end()) {
      exports[name] = it->second;
    }
  }

  // Save exports for future re-use
  if (!exportNames.empty()) {
    auto* exportObj = new ObjInstance(nullptr);
    objects.push_back(exportObj);
    for (auto& [k, v] : exports) exportObj->fields[k] = v;
    globals["__exports__" + resolvedPath] = OBJ_VAL(exportObj);
  }

  // Mark as loaded
  globals["__loaded__" + resolvedPath] = BOOL_VAL(true);
  return { InterpretResult::OK, "" };
}

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <format>
#include <iostream>
#include <sstream>

Vm::Vm()
    : stackTop(stack), frameCount(0)
{
  defineNative("print", nativePrint);
  defineNative("clock", nativeClock);
  defineNative("typeOf", nativeTypeOf);
  defineNative("input", nativeInput);
  defineNative("Object", nativeObject);
}

Vm::~Vm()
{
  for (auto* obj : objects) {
    delete obj;
  }
}

void Vm::push(Value value)
{
  *stackTop = value;
  stackTop++;
}

Value Vm::pop()
{
  stackTop--;
  return *stackTop;
}

Value Vm::peek(int distance)
{
  return stackTop[-1 - distance];
}

void Vm::runtimeError(const std::string& format, ...)
{
  va_list args;
  va_start(args, format);
  std::vfprintf(stderr, format.c_str(), args);
  va_end(args);
  std::fprintf(stderr, "\n");

  for (int i = frameCount - 1; i >= 0; --i) {
    CallFrame* frame = &frames[i];
    auto* fn = frame->closure->function;
    int instruction = static_cast<int>(frame->ip - fn->chunk.bytecode.data()) - 1;
    if (instruction < 0) instruction = 0;
    if (fn->chunk.lines.empty()) continue;
    if (instruction < static_cast<int>(fn->chunk.lines.size())) {
      int line = fn->chunk.lines[instruction];
      std::fprintf(stderr, "[line %d] in %s()\n", line, fn->name.c_str());
    }
  }

  stackTop = stack;
}

void Vm::defineNative(const std::string& name, NativeFn fn)
{
  auto* native = new ObjNative(fn, name);
  objects.push_back(native);
  globals[name] = Value(native);
}

InterpretResult Vm::interpret(const std::string& source, const std::string& filename)
{

  // Parse source to find and resolve imports
  Lexer lexer(source, filename);
  auto tokens = lexer.tokenize();
  Parser parser(tokens);
  auto statements = parser.parse();

  // Resolve imports first
  for (auto& stmt : statements) {
    if (auto* importStmt = dynamic_cast<ImportStmt*>(stmt.get())) {
      std::map<std::string, Value> exports;
      InterpretResult r = resolveAndExecuteImport(importStmt->modulePath, filename, exports);
      if (r.status != InterpretResult::OK) {
        return r;
      }
      // Handle import type
      if (importStmt->importAll && !exports.empty()) {
        // import * from "mod" → all exports go directly into globals
        for (auto& [k, v] : exports) globals[k] = v;
      } else if (importStmt->isNamespaceImport && !importStmt->names.empty()) {
        // import mod from "mod" → module object with namespace name
        auto* obj = new ObjInstance(nullptr);
        objects.push_back(obj);
        for (auto& [k, v] : exports) obj->fields[k] = v;
        globals[importStmt->names[0].lexeme] = OBJ_VAL(obj);
      } else if (!importStmt->names.empty()) {
        // import { a, b } from "mod" → selective exports
        for (auto& name : importStmt->names) {
          auto it = exports.find(name.lexeme);
          if (it != exports.end()) {
            globals[name.lexeme] = it->second;
          }
        }
      }
      // Bare import (import mod;) → just execute, no exports needed
    }
  }

  // Compile and execute main script
  Compiler compiler;
  ObjFunction* fn = compiler.compileAST(statements);
  if (fn == nullptr) {
    return { InterpretResult::COMPILE_ERROR, "Compilation failed" };
  }

  objects.insert(objects.end(), compiler.objects.begin(), compiler.objects.end());

  auto* closure = new ObjClosure(fn);
  objects.push_back(closure);

  push(OBJ_VAL(closure));
  if (fn->upvalueCount > 0) {
    push(OBJ_VAL(fn)); // placeholder
  }

  CallFrame* frame = &frames[frameCount++];
  frame->closure = closure;
  frame->ip = fn->chunk.bytecode.data();
  frame->slots = stackTop - 1;

  return run();
}

InterpretResult Vm::run()
{
  CallFrame* frame = &frames[frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
  static_cast<uint16_t>((frame->ip[0] << 8) | frame->ip[1]); \
  frame->ip += 2;
#define READ_CONSTANT() (frame->closure->function->chunk.constants[READ_BYTE()])
#define BINARY_OP(op, type) \
  do { \
    if (!peek(0).isNumber() || !peek(1).isNumber()) { \
      runtimeError("Operands must be numbers"); \
      return { InterpretResult::RUNTIME_ERROR, "Type error" }; \
    } \
    double b = pop().asNumber(); \
    double a = pop().asNumber(); \
    push(NUMBER_VAL(a op b)); \
  } while (false)

  for (;;) {
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
      case OP_CONSTANT: {
        Value constant = READ_CONSTANT();
        push(constant);
        break;
      }
      case OP_NIL: push(NIL_VAL()); break;
      case OP_TRUE: push(BOOL_VAL(true)); break;
      case OP_FALSE: push(BOOL_VAL(false)); break;
      case OP_POP: pop(); break;
      case OP_GET_LOCAL: {
        uint8_t slot = READ_BYTE();
        push(frame->slots[slot]);
        break;
      }
      case OP_SET_LOCAL: {
        uint8_t slot = READ_BYTE();
        frame->slots[slot] = peek(0);
        break;
      }
      case OP_GET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        push(frame->closure->captured[slot]);
        break;
      }
      case OP_SET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        frame->closure->captured[slot] = peek(0);
        break;
      }
      case OP_GET_GLOBAL: {
        std::string name = READ_CONSTANT().toString();
        auto it = globals.find(name);
        if (it == globals.end()) {
          runtimeError("Undefined variable '%s'", name.c_str());
          return { InterpretResult::RUNTIME_ERROR, "Undefined variable" };
        }
        push(it->second);
        break;
      }
      case OP_SET_GLOBAL: {
        std::string name = READ_CONSTANT().toString();
        auto it = globals.find(name);
        if (it == globals.end()) {
          runtimeError("Undefined variable '%s'", name.c_str());
          return { InterpretResult::RUNTIME_ERROR, "Undefined variable" };
        }
        it->second = peek(0);
        break;
      }
      case OP_EQUAL: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(a.equals(b)));
        break;
      }
      case OP_GREATER:  BINARY_OP(>, double); break;
      case OP_LESS:     BINARY_OP(<, double); break;
      case OP_ADD: {
        if (peek(0).isNumber() && peek(1).isNumber()) {
          double b = pop().asNumber();
          double a = pop().asNumber();
          push(NUMBER_VAL(a + b));
        } else if (peek(0).isObj() && peek(0).asObj()->type == ObjType::STRING &&
                   peek(1).isObj() && peek(1).asObj()->type == ObjType::STRING) {
          auto* b = static_cast<ObjString*>(pop().asObj());
          auto* a = static_cast<ObjString*>(peek(0).asObj());
          auto* result = copyString(a->str + b->str);
          objects.push_back(result);
          pop();
          push(OBJ_VAL(result));
        } else if ((peek(0).isObj() && peek(0).asObj()->type == ObjType::STRING) ||
                   (peek(1).isObj() && peek(1).asObj()->type == ObjType::STRING)) {
          // Coerce non-string operand to string
          Value b = pop();
          Value a = pop();
          std::string aStr = a.isObj() && a.asObj()->type == ObjType::STRING
            ? static_cast<ObjString*>(a.asObj())->str : a.toString();
          std::string bStr = b.isObj() && b.asObj()->type == ObjType::STRING
            ? static_cast<ObjString*>(b.asObj())->str : b.toString();
          auto* result = copyString(aStr + bStr);
          objects.push_back(result);
          push(OBJ_VAL(result));
        } else {
          runtimeError("Operands must be two numbers or two strings");
          return { InterpretResult::RUNTIME_ERROR, "Type error" };
        }
        break;
      }
      case OP_SUBTRACT: BINARY_OP(-, double); break;
      case OP_MULTIPLY: BINARY_OP(*, double); break;
      case OP_DIVIDE:   BINARY_OP(/, double); break;
      case OP_MODULO: {
        if (!peek(0).isNumber() || !peek(1).isNumber()) {
          runtimeError("Operands must be numbers");
          return { InterpretResult::RUNTIME_ERROR, "Type error" };
        }
        double b = pop().asNumber();
        double a = pop().asNumber();
        push(NUMBER_VAL(fmod(a, b)));
        break;
      }
      case OP_NOT: {
        push(BOOL_VAL(!pop().isTruthy()));
        break;
      }
      case OP_NEGATE: {
        if (!peek(0).isNumber()) {
          runtimeError("Operand must be a number");
          return { InterpretResult::RUNTIME_ERROR, "Type error" };
        }
        push(NUMBER_VAL(-pop().asNumber()));
        break;
      }
      case OP_PRINT: {
        std::cout << pop().toString() << "\n";
        break;
      }
      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        frame->ip += offset;
        break;
      }
      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();
        if (!peek(0).isTruthy()) {
          frame->ip += offset;
        }
        break;
      }
      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        frame->ip -= offset;
        break;
      }
      case OP_CALL: {
        uint8_t argCount = READ_BYTE();
        Value callee = peek(argCount);
          if (callee.isObj()) {
            switch (callee.asObj()->type) {
              case ObjType::NATIVE: {
                auto* native = static_cast<ObjNative*>(callee.asObj());
                std::vector<Value> args;
                for (int i = argCount - 1; i >= 0; --i) {
                  args.push_back(stackTop[-1 - i]);
                }
                stackTop -= argCount;
                pop();
                Value result = native->function(args);
                if (result.isObj()) {
                  objects.push_back(result.asObj());
                }
                push(result);
                break;
              }
              case ObjType::CLOSURE: {
                auto* closure = static_cast<ObjClosure*>(callee.asObj());
                if (closure->function->arity != argCount) {
                  runtimeError("Expected %d arguments but got %d",
                               closure->function->arity, argCount);
                  return { InterpretResult::RUNTIME_ERROR, "Arity mismatch" };
                }
                if (frameCount == FRAMES_MAX) {
                  runtimeError("Stack overflow");
                  return { InterpretResult::RUNTIME_ERROR, "Stack overflow" };
                }
                CallFrame* newFrame = &frames[frameCount++];
                newFrame->closure = closure;
                newFrame->ip = closure->function->chunk.bytecode.data();
                newFrame->slots = stackTop - 1 - argCount;
                frame = newFrame;
                break;
              }
              case ObjType::CLASS: {
                auto* klass = static_cast<ObjClass*>(callee.asObj());
                auto* instance = new ObjInstance(klass);
                objects.push_back(instance);
                auto ctorIt = klass->methods.find("constructor");
                if (ctorIt != klass->methods.end()) {
                  auto* ctor = ctorIt->second;
                  if (frameCount == FRAMES_MAX) {
                    runtimeError("Stack overflow");
                    return { InterpretResult::RUNTIME_ERROR, "Stack overflow" };
                  }
                  // Save original arguments
                  std::vector<Value> savedArgs;
                  for (int i = 0; i < argCount; i++) {
                    savedArgs.push_back(stackTop[-argCount + i]);
                  }
                  // Rebuild stack: [instance, arg1, ..., argN]
                  stackTop -= argCount + 1; // remove class + args
                  push(OBJ_VAL(instance));
                  for (auto& a : savedArgs) push(a);
                  CallFrame* newFrame = &frames[frameCount++];
                  newFrame->closure = ctor;
                  newFrame->ip = ctor->function->chunk.bytecode.data();
                  // slots[0] = instance, slots[1] = arg1, ...
                  newFrame->slots = stackTop - 1 - argCount;
                  frame = newFrame;
                } else {
                  stackTop -= argCount;
                  pop();
                  push(OBJ_VAL(instance));
                }
                break;
              }
              case ObjType::BOUND_METHOD: {
                auto* bound = static_cast<ObjBoundMethod*>(callee.asObj());
                if (frameCount == FRAMES_MAX) {
                  runtimeError("Stack overflow");
                  return { InterpretResult::RUNTIME_ERROR, "Stack overflow" };
                }
                CallFrame* newFrame = &frames[frameCount++];
                newFrame->closure = bound->method;
                newFrame->ip = bound->method->function->chunk.bytecode.data();
                stackTop[-1 - argCount] = bound->receiver;
                newFrame->slots = stackTop - 1 - argCount;
                frame = newFrame;
                break;
              }
              default:
                runtimeError("Can only call functions and classes");
                return { InterpretResult::RUNTIME_ERROR, "Type error" };
            }
          } else {
            runtimeError("Can only call functions and classes");
            return { InterpretResult::RUNTIME_ERROR, "Type error" };
          }
        break;
      }
      case OP_CLOSURE: {
        auto* fn = static_cast<ObjFunction*>(READ_CONSTANT().asObj());
        auto* closure = new ObjClosure(fn);
        objects.push_back(closure);
        for (int i = 0; i < fn->upvalueCount; i++) {
          bool isLocal = READ_BYTE() != 0;
          uint8_t index = READ_BYTE();
          if (isLocal) {
            closure->captured[i] = frame->slots[index];
          } else {
            closure->captured[i] = frame->closure->captured[index];
          }
        }
        push(OBJ_VAL(closure));
        break;
      }
      case OP_CLOSE_UPVALUE: {
        pop();
        break;
      }
      case OP_RETURN: {
        bool isCtor = frame->closure->function->name == "constructor";
        Value result = pop();
        frameCount--;
        if (frameCount == 0) {
          pop();
          return { InterpretResult::OK, "" };
        }
        stackTop = frame->slots;
        push(isCtor ? frame->slots[0] : result);
        frame = &frames[frameCount - 1];
        break;
      }
      case OP_CLASS: {
        std::string name = READ_CONSTANT().toString();
        auto* klass = new ObjClass(name);
        objects.push_back(klass);
        push(OBJ_VAL(klass));
        break;
      }
      case OP_INHERIT: {
        Value superclass = peek(1);
        if (!superclass.isObj() || superclass.asObj()->type != ObjType::CLASS) {
          runtimeError("Superclass must be a class");
          return { InterpretResult::RUNTIME_ERROR, "Type error" };
        }
        break;
      }
      case OP_METHOD: {
        std::string name = READ_CONSTANT().toString();
        Value methodVal = peek(0);
        Value classVal = peek(1);
        if (classVal.isObj() && classVal.asObj()->type == ObjType::CLASS
            && methodVal.isObj() && methodVal.asObj()->type == ObjType::CLOSURE) {
          auto* klass = static_cast<ObjClass*>(classVal.asObj());
          auto* closure = static_cast<ObjClosure*>(methodVal.asObj());
          klass->methods[name] = closure;
        }
        pop();
        break;
      }
      case OP_DUP: {
        push(peek(0));
        break;
      }
      case OP_GET_PROPERTY: {
        std::string name = READ_CONSTANT().toString();
        Value receiver = peek(0);
        if (name == "type") {
          Value captured = pop();
          auto* native = new ObjNative(
              [captured](const std::vector<Value>&) -> Value {
                return OBJ_VAL(copyString(captured.typeString()));
              },
              ".type");
          objects.push_back(native);
          push(OBJ_VAL(native));
          break;
        }
        if (receiver.isObj() && receiver.asObj()->type == ObjType::INSTANCE) {
          auto* instance = static_cast<ObjInstance*>(receiver.asObj());
          auto it = instance->fields.find(name);
          if (it != instance->fields.end()) {
            pop();
            push(it->second);
            break;
          }
          if (instance->klass) {
            auto meth = instance->klass->methods.find(name);
            if (meth != instance->klass->methods.end()) {
              pop();
              auto* bound = new ObjBoundMethod(receiver, meth->second);
              objects.push_back(bound);
              push(OBJ_VAL(bound));
              break;
            }
          }
          runtimeError("Undefined property '%s'", name.c_str());
          return { InterpretResult::RUNTIME_ERROR, "Undefined property" };
        }
        if (receiver.isObj() && receiver.asObj()->type == ObjType::STRING) {
          auto* s = static_cast<ObjString*>(receiver.asObj());
          if (name == "length") {
            pop();
            push(NUMBER_VAL(static_cast<double>(s->str.size())));
            break;
          }
        }
        runtimeError("Only instances have properties");
        return { InterpretResult::RUNTIME_ERROR, "Type error" };
      }
      case OP_SET_PROPERTY: {
        std::string name = READ_CONSTANT().toString();
        Value val = peek(0);
        Value obj = peek(1);
        if (!obj.isObj() || obj.asObj()->type != ObjType::INSTANCE) {
          runtimeError("Only instances have properties");
          return { InterpretResult::RUNTIME_ERROR, "Type error" };
        }
        auto* instance = static_cast<ObjInstance*>(obj.asObj());
        instance->fields[name] = val;
        pop();
        pop();
        push(val);
        break;
      }
      case OP_INVOKE: {
        uint8_t method = READ_BYTE();
        uint8_t argCount = READ_BYTE();
        break;
      }
      case OP_ARRAY: {
        uint8_t count = READ_BYTE();
        std::vector<Value> elements;
        for (int i = count - 1; i >= 0; --i) {
          elements.push_back(stackTop[-1 - i]);
        }
        stackTop -= count;
        auto* arr = new ObjArray(std::move(elements));
        objects.push_back(arr);
        push(OBJ_VAL(arr));
        break;
      }
      case OP_OBJECT: {
        auto* obj = new ObjInstance(nullptr);
        objects.push_back(obj);
        push(OBJ_VAL(obj));
        break;
      }
      case OP_OBJECT_BUILD: {
        uint8_t count = READ_BYTE();
        auto* obj = new ObjInstance(nullptr);
        objects.push_back(obj);
        for (int i = count - 1; i >= 0; --i) {
          std::string key = READ_CONSTANT().toString();
          obj->fields[key] = pop();
        }
        push(OBJ_VAL(obj));
        break;
      }
      case OP_INDEX_GET: {
        Value index = pop();
        Value target = pop();
        if (!index.isNumber()) {
          runtimeError("Index must be a number");
          return { InterpretResult::RUNTIME_ERROR, "Type error" };
        }
        if (target.isObj() && target.asObj()->type == ObjType::ARRAY) {
          auto* arr = static_cast<ObjArray*>(target.asObj());
          int i = static_cast<int>(index.asNumber());
          if (i < 0 || i >= static_cast<int>(arr->elements.size())) {
            runtimeError("Index out of bounds");
            return { InterpretResult::RUNTIME_ERROR, "Index error" };
          }
          push(arr->elements[i]);
        } else {
          runtimeError("Can only index arrays");
          return { InterpretResult::RUNTIME_ERROR, "Type error" };
        }
        break;
      }
      case OP_INDEX_SET: {
        Value val = pop();
        Value index = pop();
        Value target = pop();
        if (!index.isNumber()) {
          runtimeError("Index must be a number");
          return { InterpretResult::RUNTIME_ERROR, "Type error" };
        }
        if (target.isObj() && target.asObj()->type == ObjType::ARRAY) {
          auto* arr = static_cast<ObjArray*>(target.asObj());
          int i = static_cast<int>(index.asNumber());
          if (i < 0 || i >= static_cast<int>(arr->elements.size())) {
            runtimeError("Index out of bounds");
            return { InterpretResult::RUNTIME_ERROR, "Index error" };
          }
          arr->elements[i] = val;
          push(val);
        } else {
          runtimeError("Can only index arrays");
          return { InterpretResult::RUNTIME_ERROR, "Type error" };
        }
        break;
      }
      case OP_DEFINE_GLOBAL: {
        std::string name = READ_CONSTANT().toString();
        globals[name] = peek(0);
        pop();
        break;
      }
      case OP_PUSH_NULL:
        push(NIL_VAL());
        break;
    }
  }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef BINARY_OP
}

Value Vm::nativePrint(const std::vector<Value>& args)
{
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) std::cout << " ";
    std::cout << args[i].toString();
  }
  std::cout << "\n";
  return NIL_VAL();
}

Value Vm::nativeClock(const std::vector<Value>& args)
{
  (void)args;
  auto now = std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();
  double seconds = std::chrono::duration<double>(duration).count();
  return NUMBER_VAL(seconds);
}

Value Vm::nativeTypeOf(const std::vector<Value>& args)
{
  if (args.empty()) {
    auto* str = copyString("undefined");
    return OBJ_VAL(str);
  }
  auto* str = copyString(args[0].typeString());
  return OBJ_VAL(str);
}

Value Vm::nativeInput(const std::vector<Value>& args)
{
  if (!args.empty()) {
    std::cout << args[0].toString() << std::flush;
  }
  std::string line;
  if (!std::getline(std::cin, line)) {
    auto* str = copyString("");
    return OBJ_VAL(str);
  }
  auto* str = copyString(line);
  return OBJ_VAL(str);
}

Value Vm::nativeObject(const std::vector<Value>& args)
{
  if (args.empty()) {
    auto* obj = new ObjInstance(nullptr);
    return OBJ_VAL(obj);
  }
  return args[0];
}
