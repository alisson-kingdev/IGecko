#ifndef VM_VM_H
#define VM_VM_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Chunk.h"
#include "Object.h"
#include "Value.h"

struct Stmt;

struct CallFrame
{
  ObjClosure* closure;
  uint8_t* ip;
  Value* slots;
};

constexpr int STACK_MAX = 256 * 1024;
constexpr int FRAMES_MAX = 1024;

struct InterpretResult
{
  enum Status { OK, COMPILE_ERROR, RUNTIME_ERROR };
  Status status;
  std::string message;
};

class Vm
{
public:
  Vm();
  ~Vm();

  InterpretResult interpret(const std::string& source, const std::string& filename);

private:
  Value stack[STACK_MAX];
  Value* stackTop;
  CallFrame frames[FRAMES_MAX];
  int frameCount;
  std::map<std::string, Value> globals;
  std::vector<Obj*> objects;

  void push(Value value);
  Value pop();
  Value peek(int distance);
  void runtimeError(const std::string& format, ...);
  void defineNative(const std::string& name, NativeFn fn);
  Value* captureUpvalues(int localIndex);

  InterpretResult run();
  InterpretResult resolveAndExecuteImport(const std::string& path, const std::string& currentFile,
                                          std::map<std::string, Value>& exports);
  std::string resolveModulePath(const std::string& path, const std::string& currentFile);
  std::string readFile(const std::string& path);

  static Value nativePrint(const std::vector<Value>& args);
  static Value nativeClock(const std::vector<Value>& args);
  static Value nativeTypeOf(const std::vector<Value>& args);
  static Value nativeInput(const std::vector<Value>& args);
  static Value nativeObject(const std::vector<Value>& args);

  friend struct Compiler;
};

#endif
