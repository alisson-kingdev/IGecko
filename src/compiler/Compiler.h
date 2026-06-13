#ifndef COMPILER_H
#define COMPILER_H

#include <memory>
#include <string>
#include <vector>

#include "../ast/Nodes.h"
#include "../lexer/Lexer.h"
#include "../parser/Parser.h"
#include "../vm/Chunk.h"
#include "../vm/Object.h"
#include "../vm/Value.h"

struct Local
{
  std::string name;
  int depth;
  bool isCaptured;
};

struct Upvalue
{
  uint8_t index;
  bool isLocal;
};

struct CompilerState
{
  ObjFunction* function;
  CompilerState* enclosing;
  std::vector<Local> locals;
  std::vector<Upvalue> upvalues;
  int scopeDepth;
  int loopStart;
  int loopEnd;
  std::vector<int> breakJumps;
  bool isInLoop;
  bool hasReturned;
};

class Compiler
  : public ExprVisitor
  , public StmtVisitor
{
public:
  Compiler();
  ObjFunction* compile(const std::string& source);
  ObjFunction* compileAST(const std::vector<std::shared_ptr<Stmt>>& statements);

  std::vector<Obj*> objects;

  // ExprVisitor
  void visitBinaryExpr(BinaryExpr* expr) override;
  void visitGroupingExpr(GroupingExpr* expr) override;
  void visitLiteralExpr(LiteralExpr* expr) override;
  void visitUnaryExpr(UnaryExpr* expr) override;
  void visitVariableExpr(VariableExpr* expr) override;
  void visitAssignExpr(AssignExpr* expr) override;
  void visitLogicalExpr(LogicalExpr* expr) override;
  void visitCallExpr(CallExpr* expr) override;
  void visitGetExpr(GetExpr* expr) override;
  void visitSetExpr(SetExpr* expr) override;
  void visitThisExpr(ThisExpr* expr) override;
  void visitTemplateExpr(TemplateExpr* expr) override;
  void visitArrayExpr(ArrayExpr* expr) override;
  void visitObjectExpr(ObjectExpr* expr) override;
  void visitFunctionExpr(FunctionExpr* expr) override;
  void visitTernaryExpr(TernaryExpr* expr) override;
  void visitAwaitExpr(AwaitExpr* expr) override;
  void visitIncrementExpr(IncrementExpr* expr) override;

  // StmtVisitor
  void visitExpressionStmt(ExpressionStmt* stmt) override;
  void visitPrintStmt(PrintStmt* stmt) override;
  void visitVarStmt(VarStmt* stmt) override;
  void visitBlockStmt(BlockStmt* stmt) override;
  void visitIfStmt(IfStmt* stmt) override;
  void visitWhileStmt(WhileStmt* stmt) override;
  void visitForStmt(ForStmt* stmt) override;
  void visitReturnStmt(ReturnStmt* stmt) override;
  void visitBreakStmt(BreakStmt* stmt) override;
  void visitFunctionStmt(FunctionStmt* stmt) override;
  void visitClassStmt(ClassStmt* stmt) override;
  void visitTryStmt(TryStmt* stmt) override;
  void visitPackageStmt(PackageStmt* stmt) override;
  void visitExportStmt(ExportStmt* stmt) override;
  void visitImportStmt(ImportStmt* stmt) override;

private:
  Chunk* chunk;
  std::vector<Token> tokens;
  int current;
  CompilerState* currentState;
  CompilerState* rootState;
  int currentLine;
  bool hadError;

  // Parser helpers (for compiling AST directly, we just need current token info)
  Token peek();
  Token previous();
  bool check(TokenType type);
  bool match(TokenType type);

  // Compilation helpers
  void emitByte(uint8_t byte);
  void emitBytes(uint8_t a, uint8_t b);
  void emitReturn();
  void emitConstant(Value value);
  int emitJump(uint8_t op);
  void patchJump(int offset);
  void emitLoop(int loopStart);
  int makeConstant(Value value);

  // Variable resolution
  int resolveLocal(const std::string& name);
  int resolveUpvalue(const std::string& name);
  int addUpvalue(CompilerState* state, uint8_t index, bool isLocal);
  void declareVariable(const std::string& name);
  void addLocal(const std::string& name);
  int defineVariable(const std::string& name);
  void namedVariable(const std::string& name, bool canAssign, int line);
  void markInitialized();

  // Scope management
  void beginScope();
  void endScope();
  int localCount();

  // Compilation of AST nodes
  void compileNode(Expr* node);
  void compileStmt(Stmt* node);
  void compileFunction(CompilerState* state, FunctionExpr* expr, const std::string& name);
  void functionStmt(FunctionStmt* stmt, const std::string& name);
  void blockBody(const std::vector<std::shared_ptr<Stmt>>& statements);

  // Error handling
  void error(const std::string& message);
  void errorAt(const Token& token, const std::string& message);

  // Parser state
  std::string sourceStr;
  int parserPos;

  void advance();
  void consume(TokenType type, const std::string& message);
};

#endif
