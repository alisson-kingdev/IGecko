#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <any>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "../ast/Nodes.h"
#include "environment/Environment.h"
#include "runtime/Runtime.h"

/**
 * @brief The main execution engine for the IGecko language.
 *
 * Implements the Visitor pattern for both expression and statement nodes to
 * evaluate and execute the Abstract Syntax Tree (AST).
 */
class Interpreter
    : public ExprVisitor
    , public StmtVisitor
{
public:
  /** @brief Points to the currently executing interpreter (set during interpret()). */
  static thread_local Interpreter* current;
  /** @brief Constructs an interpreter with the context of a source file. */
  explicit Interpreter(std::string filename = "<main>", std::ostream* out = &std::cout);

  /** @brief Interprets a list of AST statements. */
  void interpret(const std::vector<std::shared_ptr<Stmt>>& statements);

  std::ostream* outStream;

  /** @brief Executes a list of statements within the current scope. */
  void executeStatements(const std::vector<std::shared_ptr<Stmt>>& statements);

  std::shared_ptr<Environment> globals;
  std::shared_ptr<Environment> environment;
  std::shared_ptr<ModuleCache> moduleCache;
  std::string currentPackage;

  // --- ExprVisitor implementation ---
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

  // --- StmtVisitor implementation ---
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

  /** @brief Executes a block of statements within a given environment (public for native modules).
   */
  void executeBlock(const std::vector<std::shared_ptr<Stmt>>& statements,
                    std::shared_ptr<Environment> env);

  /** @brief Calls a user-defined Gecko function (public for native modules). */
  gecko::Value callUserFunction(const std::shared_ptr<UserFunction>& func,
                                const std::vector<gecko::Value>& arguments,
                                std::shared_ptr<Environment> env,
                                const gecko::Value& thisInstance = nullptr);

private:
  std::string sourceFile;
  gecko::Value result;
  bool inFunction = false;
  int loopDepth = 0;
  int recursionDepth = 0;
  static constexpr int MAX_RECURSION_DEPTH = 1000;
  std::vector<gecko::Value> thisStack;

  gecko::Value evaluate(Expr* expr);
  void execute(Stmt* stmt);
  void defineBuiltins();
  std::string stringify(const gecko::Value& value);
  bool isTruthy(const gecko::Value& value);
  gecko::Value binaryOp(const gecko::Value& left, const Token& op, const gecko::Value& right);
  gecko::Value unaryOp(const Token& op, const gecko::Value& right);
  gecko::Value callFunction(const gecko::Value& callee, const std::vector<gecko::Value>& arguments);
  std::string getType(const gecko::Value& value);
  std::string decodeStringLiteral(const std::string& lexeme, bool quoted = true);
};

#endif // INTERPRETER_H
