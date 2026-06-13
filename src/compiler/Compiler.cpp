#include "Compiler.h"

#include <format>
#include <iostream>
#include <sstream>

Compiler::Compiler()
    : current(0), currentState(nullptr), rootState(nullptr), currentLine(0), hadError(false), parserPos(0)
{}

Token Compiler::peek()
{
  return tokens[current];
}

Token Compiler::previous()
{
  return tokens[current - 1];
}

bool Compiler::check(TokenType type)
{
  return tokens[current].type == type;
}

bool Compiler::match(TokenType type)
{
  if (check(type)) {
    advance();
    return true;
  }
  return false;
}

void Compiler::advance()
{
  if (current < static_cast<int>(tokens.size())) {
    current++;
  }
}

void Compiler::consume(TokenType type, const std::string& message)
{
  if (tokens[current].type == type) {
    advance();
  } else {
    errorAt(tokens[current], message);
  }
}

void Compiler::error(const std::string& message)
{
  if (tokens.empty()) {
    hadError = true;
    std::cerr << "Error: " << message << std::endl;
    return;
  }
  if (current > 0) {
    errorAt(previous(), message);
  } else {
    errorAt(peek(), message);
  }
}

void Compiler::errorAt(const Token& token, const std::string& message)
{
  hadError = true;
  std::cerr << std::format("[line {}] Error: {}\n", token.line, message);
}

// --- Bytecode emission ---

void Compiler::emitByte(uint8_t byte)
{
  chunk->write(byte, currentLine);
}

void Compiler::emitBytes(uint8_t a, uint8_t b)
{
  emitByte(a);
  emitByte(b);
}

void Compiler::emitReturn()
{
  emitByte(OP_NIL);
  emitByte(OP_RETURN);
}

void Compiler::emitConstant(Value value)
{
  int index = makeConstant(value);
  emitByte(OP_CONSTANT);
  emitByte(static_cast<uint8_t>(index));
}

int Compiler::makeConstant(Value value)
{
  return chunk->addConstant(value);
}

int Compiler::emitJump(uint8_t op)
{
  emitByte(op);
  emitByte(0xff);
  emitByte(0xff);
  return chunk->count() - 2;
}

void Compiler::patchJump(int offset)
{
  int jump = chunk->count() - offset - 2;
  if (jump > 65535) {
    error("Too much code to jump over");
    return;
  }
  chunk->bytecode[offset] = static_cast<uint8_t>((jump >> 8) & 0xff);
  chunk->bytecode[offset + 1] = static_cast<uint8_t>(jump & 0xff);
  chunk->lines[offset] = currentLine;
  chunk->lines[offset + 1] = currentLine;
}

void Compiler::emitLoop(int loopStart)
{
  emitByte(OP_LOOP);
  int offset = chunk->count() - loopStart + 2;
  if (offset > 65535) {
    error("Loop body too large");
    return;
  }
  emitByte(static_cast<uint8_t>((offset >> 8) & 0xff));
  emitByte(static_cast<uint8_t>(offset & 0xff));
}

// --- Scope management ---

void Compiler::beginScope()
{
  currentState->scopeDepth++;
}

void Compiler::endScope()
{
  currentState->scopeDepth--;
  while (!currentState->locals.empty() &&
         currentState->locals.back().depth > currentState->scopeDepth) {
    if (currentState->locals.back().isCaptured) {
      emitByte(OP_CLOSE_UPVALUE);
    } else {
      emitByte(OP_POP);
    }
    currentState->locals.pop_back();
  }
}

int Compiler::localCount()
{
  return static_cast<int>(currentState->locals.size());
}

void Compiler::declareVariable(const std::string& name)
{
  if (currentState->scopeDepth == 0) return;
  for (int i = localCount() - 1; i >= 0; --i) {
    auto* local = &currentState->locals[i];
    if (local->depth != -1 && local->depth < currentState->scopeDepth) break;
    if (local->name == name) {
      error("Already a variable with this name in this scope");
      return;
    }
  }
  addLocal(name);
}

void Compiler::addLocal(const std::string& name)
{
  currentState->locals.push_back({name, -1, false});
}

void Compiler::markInitialized()
{
  if (currentState->scopeDepth == 0) return;
  currentState->locals.back().depth = currentState->scopeDepth;
}

int Compiler::defineVariable(const std::string& name)
{
  if (currentState->scopeDepth > 0) {
    markInitialized();
    return 0;
  }
  int index = makeConstant(OBJ_VAL(copyString(name)));
  objects.push_back(static_cast<Obj*>(chunk->constants[index].asObj()));
  emitBytes(OP_DEFINE_GLOBAL, static_cast<uint8_t>(index));
  return index;
}

int Compiler::resolveLocal(const std::string& name)
{
  for (int i = localCount() - 1; i >= 0; --i) {
    if (currentState->locals[i].name == name) {
      return i;
    }
  }
  return -1;
}

int Compiler::resolveUpvalue(const std::string& name)
{
  if (currentState->enclosing == nullptr) return -1;
  int local = -1;
  for (int i = static_cast<int>(currentState->enclosing->locals.size()) - 1; i >= 0; --i) {
    if (currentState->enclosing->locals[i].name == name) {
      local = i;
      currentState->enclosing->locals[i].isCaptured = true;
      return addUpvalue(currentState, static_cast<uint8_t>(local), true);
    }
  }
  // Recursively search enclosing scopes by temporarily switching currentState
  CompilerState* saved = currentState;
  currentState = currentState->enclosing;
  int upvalue = resolveUpvalue(name);
  currentState = saved;
  if (upvalue != -1) {
    return addUpvalue(currentState, static_cast<uint8_t>(upvalue), false);
  }
  return -1;
}

int Compiler::addUpvalue(CompilerState* state, uint8_t index, bool isLocal)
{
  for (int i = 0; i < static_cast<int>(state->upvalues.size()); ++i) {
    if (state->upvalues[i].index == index && state->upvalues[i].isLocal == isLocal) {
      return i;
    }
  }
  state->upvalues.push_back({index, isLocal});
  return static_cast<int>(state->upvalues.size()) - 1;
}

void Compiler::namedVariable(const std::string& name, bool canAssign, int line)
{
  int arg = resolveLocal(name);
  if (arg != -1) {
    if (canAssign && match(TokenType::EQUAL)) {
      compileNode(static_cast<Expr*>(nullptr));
      emitBytes(OP_SET_LOCAL, static_cast<uint8_t>(arg));
    } else {
      emitBytes(OP_GET_LOCAL, static_cast<uint8_t>(arg));
    }
    return;
  }

  arg = resolveUpvalue(name);
  if (arg != -1) {
    if (canAssign && match(TokenType::EQUAL)) {
      compileNode(static_cast<Expr*>(nullptr)); // placeholder
      emitBytes(OP_SET_UPVALUE, static_cast<uint8_t>(arg));
    } else {
      emitBytes(OP_GET_UPVALUE, static_cast<uint8_t>(arg));
    }
    return;
  }

  int index = makeConstant(OBJ_VAL(copyString(name)));
  objects.push_back(static_cast<Obj*>(chunk->constants[index].asObj()));
  if (canAssign && match(TokenType::EQUAL)) {
    compileNode(static_cast<Expr*>(nullptr)); // placeholder
    emitBytes(OP_SET_GLOBAL, static_cast<uint8_t>(index));
  } else {
    emitBytes(OP_GET_GLOBAL, static_cast<uint8_t>(index));
  }
}

// --- Compilation of AST nodes ---

void Compiler::compileNode(Expr* node)
{
  if (node == nullptr) return;
  node->accept(this);
}

void Compiler::compileStmt(Stmt* node)
{
  if (node == nullptr) return;
  node->accept(this);
}

// --- ExprVisitor implementation ---

void Compiler::visitLiteralExpr(LiteralExpr* expr)
{
  currentLine = expr->value.line;
  switch (expr->value.type) {
    case TokenType::NUMBER: {
      double val = std::stod(expr->value.lexeme);
      emitConstant(NUMBER_VAL(val));
      break;
    }
    case TokenType::STRING: {
      std::string raw = expr->value.lexeme;
      if (raw.size() >= 2) raw = raw.substr(1, raw.size() - 2);
      auto* str = copyString(raw);
      objects.push_back(str);
      emitConstant(OBJ_VAL(str));
      break;
    }
    case TokenType::TEMPLATE_STRING_PART: {
      auto* str = copyString(expr->value.lexeme);
      objects.push_back(str);
      emitConstant(OBJ_VAL(str));
      break;
    }
    case TokenType::TRUE:
      emitByte(OP_TRUE);
      break;
    case TokenType::FALSE:
      emitByte(OP_FALSE);
      break;
    case TokenType::NULL_VAL:
      emitByte(OP_NIL);
      break;
    default:
      break;
  }
}

void Compiler::visitBinaryExpr(BinaryExpr* expr)
{
  currentLine = expr->op.line;
  compileNode(expr->left.get());
  compileNode(expr->right.get());

  switch (expr->op.type) {
    case TokenType::PLUS:          emitByte(OP_ADD); break;
    case TokenType::MINUS:         emitByte(OP_SUBTRACT); break;
    case TokenType::MULTIPLY:      emitByte(OP_MULTIPLY); break;
    case TokenType::DIVIDE:        emitByte(OP_DIVIDE); break;
    case TokenType::MODULO:        emitByte(OP_MODULO); break;
    case TokenType::EQUAL:         emitByte(OP_EQUAL); break;
    case TokenType::GREATER:       emitByte(OP_GREATER); break;
    case TokenType::GREATER_EQUAL: emitByte(OP_LESS); emitByte(OP_NOT); break;
    case TokenType::LESS:          emitByte(OP_LESS); break;
    case TokenType::LESS_EQUAL:    emitByte(OP_GREATER); emitByte(OP_NOT); break;
    case TokenType::NOT_EQUAL:     emitByte(OP_EQUAL); emitByte(OP_NOT); break;
    default: break;
  }
}

void Compiler::visitGroupingExpr(GroupingExpr* expr)
{
  currentLine = 0;
  compileNode(expr->expression.get());
}

void Compiler::visitUnaryExpr(UnaryExpr* expr)
{
  currentLine = expr->op.line;
  compileNode(expr->right.get());
  switch (expr->op.type) {
    case TokenType::NOT: emitByte(OP_NOT); break;
    case TokenType::MINUS: emitByte(OP_NEGATE); break;
    default: break;
  }
}

void Compiler::visitVariableExpr(VariableExpr* expr)
{
  currentLine = expr->name.line;
  namedVariable(expr->name.lexeme, false, expr->name.line);
}

void Compiler::visitAssignExpr(AssignExpr* expr)
{
  currentLine = expr->name.line;
  compileNode(expr->value.get());
  int arg = resolveLocal(expr->name.lexeme);
  if (arg != -1) {
    emitBytes(OP_SET_LOCAL, static_cast<uint8_t>(arg));
  } else {
    arg = resolveUpvalue(expr->name.lexeme);
    if (arg != -1) {
      emitBytes(OP_SET_UPVALUE, static_cast<uint8_t>(arg));
    } else {
      int index = makeConstant(OBJ_VAL(copyString(expr->name.lexeme)));
      objects.push_back(static_cast<Obj*>(chunk->constants[index].asObj()));
      emitBytes(OP_SET_GLOBAL, static_cast<uint8_t>(index));
    }
  }
}

void Compiler::visitLogicalExpr(LogicalExpr* expr)
{
  currentLine = expr->op.line;
  compileNode(expr->left.get());
  if (expr->op.type == TokenType::OR) {
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_JUMP);
    int endJump = emitJump(OP_JUMP);
    patchJump(elseJump);
    emitByte(OP_POP);
    compileNode(expr->right.get());
    patchJump(endJump);
  } else {
    int endJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    compileNode(expr->right.get());
    patchJump(endJump);
  }
}

void Compiler::visitCallExpr(CallExpr* expr)
{
  currentLine = expr->paren.line;
  compileNode(expr->callee.get());
  for (auto& arg : expr->arguments) {
    compileNode(arg.get());
  }
  emitBytes(OP_CALL, static_cast<uint8_t>(expr->arguments.size()));
}

void Compiler::visitGetExpr(GetExpr* expr)
{
  currentLine = expr->name.line;
  compileNode(expr->object.get());
  int index = makeConstant(OBJ_VAL(copyString(expr->name.lexeme)));
  objects.push_back(static_cast<Obj*>(chunk->constants[index].asObj()));
  emitBytes(OP_GET_PROPERTY, static_cast<uint8_t>(index));
}

void Compiler::visitSetExpr(SetExpr* expr)
{
  currentLine = expr->name.line;
  compileNode(expr->object.get());
  int index = makeConstant(OBJ_VAL(copyString(expr->name.lexeme)));
  objects.push_back(static_cast<Obj*>(chunk->constants[index].asObj()));
  compileNode(expr->value.get());
  emitBytes(OP_SET_PROPERTY, static_cast<uint8_t>(index));
}

void Compiler::visitThisExpr(ThisExpr* expr)
{
  currentLine = expr->keyword.line;
  namedVariable("this", false, expr->keyword.line);
}

void Compiler::visitTemplateExpr(TemplateExpr* expr)
{
  currentLine = 0;
  compileNode(expr->parts[0].get());
  for (size_t i = 1; i < expr->parts.size(); i++) {
    compileNode(expr->parts[i].get());
    emitByte(OP_ADD);
  }
}

void Compiler::visitArrayExpr(ArrayExpr* expr)
{
  currentLine = 0;
  for (auto& elem : expr->elements) {
    compileNode(elem.get());
  }
  emitBytes(OP_ARRAY, static_cast<uint8_t>(expr->elements.size()));
}

void Compiler::visitObjectExpr(ObjectExpr* expr)
{
  currentLine = 0;
  if (expr->properties.empty()) {
    emitByte(OP_OBJECT);
    return;
  }
  for (auto& [key, valExpr] : expr->properties) {
    compileNode(valExpr.get());
  }
  emitByte(OP_OBJECT_BUILD);
  emitByte(static_cast<uint8_t>(expr->properties.size()));
  for (auto& [key, valExpr] : expr->properties) {
    int idx = makeConstant(OBJ_VAL(copyString(key)));
    emitByte(static_cast<uint8_t>(idx));
  }
}

void Compiler::visitFunctionExpr(FunctionExpr* expr)
{
  currentLine = 0;
  compileFunction(currentState, expr, "<anonymous>");
}

void Compiler::visitTernaryExpr(TernaryExpr* expr)
{
  currentLine = 0;
  compileNode(expr->condition.get());
  int elseJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  compileNode(expr->thenBranch.get());
  int endJump = emitJump(OP_JUMP);
  patchJump(elseJump);
  emitByte(OP_POP);
  compileNode(expr->elseBranch.get());
  patchJump(endJump);
}

void Compiler::visitAwaitExpr(AwaitExpr* expr)
{
  currentLine = 0;
  compileNode(expr->expr.get());
}

void Compiler::visitIncrementExpr(IncrementExpr* expr)
{
  currentLine = expr->name.line;
  bool isInc = expr->op.type == TokenType::PLUS_PLUS;

  namedVariable(expr->name.lexeme, false, expr->name.line);
  emitByte(OP_DUP);
  emitConstant(NUMBER_VAL(1.0));
  emitByte(isInc ? OP_ADD : OP_SUBTRACT);

  int arg = resolveLocal(expr->name.lexeme);
  if (arg != -1) {
    emitBytes(OP_SET_LOCAL, static_cast<uint8_t>(arg));
  } else {
    arg = resolveUpvalue(expr->name.lexeme);
    if (arg != -1) {
      emitBytes(OP_SET_UPVALUE, static_cast<uint8_t>(arg));
    } else {
      int index = makeConstant(OBJ_VAL(copyString(expr->name.lexeme)));
      objects.push_back(static_cast<Obj*>(chunk->constants[index].asObj()));
      emitBytes(OP_SET_GLOBAL, static_cast<uint8_t>(index));
    }
  }
}

// --- StmtVisitor implementation ---

void Compiler::visitExpressionStmt(ExpressionStmt* stmt)
{
  compileNode(stmt->expression.get());
  emitByte(OP_POP);
}

void Compiler::visitPrintStmt(PrintStmt* stmt)
{
  compileNode(stmt->expression.get());
  emitByte(OP_PRINT);
}

void Compiler::visitVarStmt(VarStmt* stmt)
{
  currentLine = stmt->name.line;
  if (stmt->initializer) {
    compileNode(stmt->initializer.get());
  } else {
    emitConstant(NUMBER_VAL(0.0));
  }
  declareVariable(stmt->name.lexeme);
  defineVariable(stmt->name.lexeme);
}

void Compiler::visitBlockStmt(BlockStmt* stmt)
{
  beginScope();
  for (auto& s : stmt->statements) {
    compileStmt(s.get());
  }
  endScope();
}

void Compiler::visitIfStmt(IfStmt* stmt)
{
  currentLine = 0;
  compileNode(stmt->condition.get());
  int thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  compileStmt(stmt->thenBranch.get());
  int elseJump = emitJump(OP_JUMP);
  patchJump(thenJump);
  emitByte(OP_POP);
  if (stmt->elseBranch) {
    compileStmt(stmt->elseBranch.get());
  }
  patchJump(elseJump);
}

void Compiler::visitWhileStmt(WhileStmt* stmt)
{
  currentLine = 0;
  int loopStart = chunk->count();
  compileNode(stmt->condition.get());
  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);

  std::vector<int> savedBreaks = std::move(currentState->breakJumps);
  currentState->breakJumps.clear();
  bool savedIsInLoop = currentState->isInLoop;
  currentState->isInLoop = true;
  int savedLoopEnd = currentState->loopEnd;
  currentState->loopEnd = -1;

  compileStmt(stmt->body.get());

  // Patch breaks
  int afterLoop = chunk->count();
  if (currentState->loopEnd != -1) afterLoop = currentState->loopEnd;
  for (int j : currentState->breakJumps) {
    int offset = afterLoop - j - 2;
    chunk->bytecode[j] = (offset >> 8) & 0xff;
    chunk->bytecode[j + 1] = offset & 0xff;
  }

  emitLoop(loopStart);
  patchJump(exitJump);
  emitByte(OP_POP);

  currentState->isInLoop = savedIsInLoop;
  currentState->loopEnd = savedLoopEnd;
  currentState->breakJumps = std::move(savedBreaks);
}

void Compiler::visitForStmt(ForStmt* stmt)
{
  beginScope();
  if (stmt->initializer) {
    compileStmt(stmt->initializer.get());
  }
  int loopStart = chunk->count();
  int exitJump = -1;
  if (stmt->condition) {
    compileNode(stmt->condition.get());
    exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
  }

  std::vector<int> savedBreaks = std::move(currentState->breakJumps);
  currentState->breakJumps.clear();
  bool savedIsInLoop = currentState->isInLoop;
  currentState->isInLoop = true;
  int savedLoopEnd = currentState->loopEnd;
  currentState->loopEnd = -1;

  compileStmt(stmt->body.get());

  int afterBody = chunk->count();
  if (currentState->loopEnd != -1) afterBody = currentState->loopEnd;
  for (int j : currentState->breakJumps) {
    int offset = afterBody - j - 2;
    chunk->bytecode[j] = (offset >> 8) & 0xff;
    chunk->bytecode[j + 1] = offset & 0xff;
  }

  if (stmt->increment) {
    compileNode(stmt->increment.get());
    emitByte(OP_POP);
  }
  emitLoop(loopStart);
  if (exitJump != -1) {
    patchJump(exitJump);
    emitByte(OP_POP);
  }
  endScope();

  currentState->isInLoop = savedIsInLoop;
  currentState->loopEnd = savedLoopEnd;
  currentState->breakJumps = std::move(savedBreaks);
}

void Compiler::visitReturnStmt(ReturnStmt* stmt)
{
  currentLine = stmt->keyword.line;
  if (currentState->function == rootState->function) {
    error("Can't return from top-level code");
    return;
  }
  if (stmt->value) {
    compileNode(stmt->value.get());
  } else {
    emitByte(OP_NIL);
  }
  emitByte(OP_RETURN);
}

void Compiler::visitBreakStmt(BreakStmt* stmt)
{
  currentLine = stmt->keyword.line;
  if (!currentState->isInLoop) {
    error("Break must be inside a loop");
    return;
  }
  int jump = emitJump(OP_JUMP);
  currentState->breakJumps.push_back(jump);
}

void Compiler::visitFunctionStmt(FunctionStmt* stmt)
{
  currentLine = stmt->name.line;
  declareVariable(stmt->name.lexeme);
  functionStmt(stmt, stmt->name.lexeme);
  defineVariable(stmt->name.lexeme);
}

void Compiler::visitClassStmt(ClassStmt* stmt)
{
  currentLine = stmt->name.line;
  declareVariable(stmt->name.lexeme);
  int nameIndex = makeConstant(OBJ_VAL(copyString(stmt->name.lexeme)));
  objects.push_back(static_cast<Obj*>(chunk->constants[nameIndex].asObj()));
  emitBytes(OP_CLASS, static_cast<uint8_t>(nameIndex));
  defineVariable(stmt->name.lexeme);

  if (stmt->superclass) {
    namedVariable(stmt->superclass->name.lexeme, false, stmt->superclass->name.line);
    if (stmt->superclass->name.lexeme == stmt->name.lexeme) {
      error("A class can't inherit from itself");
      return;
    }
    beginScope();
    addLocal("super");
    defineVariable("super");
    namedVariable(stmt->name.lexeme, false, stmt->name.line);
    emitByte(OP_INHERIT);
  }

  namedVariable(stmt->name.lexeme, false, stmt->name.line);
  for (auto& method : stmt->methods) {
    std::string methodName = method->name.lexeme;
    int methodIndex = makeConstant(OBJ_VAL(copyString(methodName)));
    objects.push_back(static_cast<Obj*>(chunk->constants[methodIndex].asObj()));
    functionStmt(method.get(), methodName);
    emitBytes(OP_METHOD, static_cast<uint8_t>(methodIndex));
  }
  emitByte(OP_POP);

  if (stmt->superclass) {
    endScope();
  }
}

void Compiler::visitTryStmt(TryStmt* stmt)
{
  (void)stmt;
  error("Try/catch not yet supported in VM");
}

void Compiler::visitPackageStmt(PackageStmt* stmt)
{
  (void)stmt;
  // Package declaration is a no-op at runtime
}

void Compiler::visitExportStmt(ExportStmt* stmt)
{
  (void)stmt;
  // Exports are handled at the Vm level, not by bytecode
}

void Compiler::visitImportStmt(ImportStmt* stmt)
{
  (void)stmt;
  // Imports are resolved at the Vm level before compilation
}

// --- Function compilation ---

void Compiler::compileFunction(CompilerState* state, FunctionExpr* expr, const std::string& name)
{
  CompilerState fnState;
  fnState.function = new ObjFunction();
  fnState.function->name = name;
  fnState.enclosing = currentState;
  fnState.scopeDepth = 0;
  fnState.hasReturned = false;
  fnState.isInLoop = false;
  fnState.loopStart = -1;
  fnState.loopEnd = -1;

  objects.push_back(fnState.function);

  Chunk* savedChunk = chunk;
  CompilerState* savedState = currentState;

  chunk = &fnState.function->chunk;
  currentState = &fnState;

  beginScope();
  fnState.locals.push_back({"this", 0, false});
  for (auto& param : expr->parameters) {
    fnState.function->arity++;
    declareVariable(param.lexeme);
    defineVariable(param.lexeme);
  }
  for (auto& stmt : expr->body) {
    compileStmt(stmt.get());
  }

  endScope();
  emitReturn();

  // Restore parent chunk and state
  chunk = savedChunk;
  currentState = savedState;

  fnState.function->upvalueCount = static_cast<int>(fnState.upvalues.size());

  int index = makeConstant(OBJ_VAL(fnState.function));
  emitBytes(OP_CLOSURE, static_cast<uint8_t>(index));

  for (auto& upvalue : fnState.upvalues) {
    emitByte(upvalue.isLocal ? 1 : 0);
    emitByte(upvalue.index);
  }
}

void Compiler::functionStmt(FunctionStmt* stmt, const std::string& name)
{
  auto* fnExpr = new FunctionExpr(stmt->parameters, stmt->body, stmt->returnType, stmt->isAsync);
  compileFunction(currentState, fnExpr, name);
  delete fnExpr;
}

void Compiler::blockBody(const std::vector<std::shared_ptr<Stmt>>& statements)
{
  beginScope();
  for (auto& stmt : statements) {
    compileStmt(stmt.get());
  }
  endScope();
}

ObjFunction* Compiler::compile(const std::string& source)
{
  hadError = false;

  // Lex and parse
  Lexer lexer(source, "<vm>");
  tokens = lexer.tokenize();

  Parser parser(tokens);
  auto statements = parser.parse();

  return compileAST(statements);
}

ObjFunction* Compiler::compileAST(const std::vector<std::shared_ptr<Stmt>>& statements)
{
  hadError = false;

  // Setup root chunk
  ObjFunction* function = new ObjFunction();
  function->name = "<script>";
  objects.push_back(function);

  chunk = &function->chunk;

  // Setup root compiler state
  CompilerState rootFn;
  rootFn.function = function;
  rootFn.enclosing = nullptr;
  rootFn.scopeDepth = 0;
  rootFn.hasReturned = false;
  rootFn.isInLoop = false;
  rootFn.loopStart = -1;
  rootFn.loopEnd = -1;

  currentState = &rootFn;
  rootState = &rootFn;

  // Reserve slot 0 for the closure (dummy local)
  rootFn.locals.push_back({"", 0, false});

  // Compile all statements
  for (auto& stmt : statements) {
    compileStmt(stmt.get());
  }
  emitReturn();

  // No copy needed — chunk already points to function->chunk

  if (hadError) {
    return nullptr;
  }

  return function;
}
