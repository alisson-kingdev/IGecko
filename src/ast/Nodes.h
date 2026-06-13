#ifndef NODES_H
#define NODES_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../lexer/Lexer.h"

class Expr;
class Stmt;
class ArrayExpr;
class ObjectExpr;

/**
 * @brief Visitor interface for expression nodes.
 */
class ExprVisitor
{
public:
  virtual ~ExprVisitor() = default;
  virtual void visitBinaryExpr(class BinaryExpr* expr) = 0;
  virtual void visitGroupingExpr(class GroupingExpr* expr) = 0;
  virtual void visitLiteralExpr(class LiteralExpr* expr) = 0;
  virtual void visitUnaryExpr(class UnaryExpr* expr) = 0;
  virtual void visitVariableExpr(class VariableExpr* expr) = 0;
  virtual void visitAssignExpr(class AssignExpr* expr) = 0;
  virtual void visitLogicalExpr(class LogicalExpr* expr) = 0;
  virtual void visitCallExpr(class CallExpr* expr) = 0;
  virtual void visitGetExpr(class GetExpr* expr) = 0;
  virtual void visitSetExpr(class SetExpr* expr) = 0;
  virtual void visitThisExpr(class ThisExpr* expr) = 0;
  virtual void visitTemplateExpr(class TemplateExpr* expr) = 0;
  virtual void visitArrayExpr(class ArrayExpr* expr) = 0;
  virtual void visitObjectExpr(class ObjectExpr* expr) = 0;
  virtual void visitFunctionExpr(class FunctionExpr* expr) = 0;
  virtual void visitTernaryExpr(class TernaryExpr* expr) = 0;
  virtual void visitAwaitExpr(class AwaitExpr* expr) = 0;
  virtual void visitIncrementExpr(class IncrementExpr* expr) = 0;
};

/**
 * @brief Base class for expression AST nodes.
 */
class Expr
{
public:
  virtual ~Expr() = default;
  virtual void accept(ExprVisitor* visitor) = 0;
};

class BinaryExpr : public Expr
{
public:
  std::shared_ptr<Expr> left;
  Token op;
  std::shared_ptr<Expr> right;

  BinaryExpr(std::shared_ptr<Expr> l, Token o, std::shared_ptr<Expr> r)
      : left(std::move(l)), op(o), right(std::move(r))
  {}

  void accept(ExprVisitor* visitor) override
  {
    visitor->visitBinaryExpr(this);
  }
};

class GroupingExpr : public Expr
{
public:
  std::shared_ptr<Expr> expression;

  explicit GroupingExpr(std::shared_ptr<Expr> expr) : expression(std::move(expr)) {}

  void accept(ExprVisitor* visitor) override
  {
    visitor->visitGroupingExpr(this);
  }
};

class LiteralExpr : public Expr
{
public:
  Token value;

  explicit LiteralExpr(Token val) : value(val) {}

  void accept(ExprVisitor* visitor) override
  {
    visitor->visitLiteralExpr(this);
  }
};

class UnaryExpr : public Expr
{
public:
  Token op;
  std::shared_ptr<Expr> right;

  UnaryExpr(Token o, std::shared_ptr<Expr> r) : op(o), right(std::move(r)) {}

  void accept(ExprVisitor* visitor) override
  {
    visitor->visitUnaryExpr(this);
  }
};

class IncrementExpr : public Expr
{
public:
  Token name;
  Token op;

  IncrementExpr(Token n, Token o) : name(n), op(o) {}

  void accept(ExprVisitor* visitor) override
  {
    visitor->visitIncrementExpr(this);
  }
};

class VariableExpr : public Expr
{
public:
  Token name;

  explicit VariableExpr(Token n) : name(n) {}

  void accept(ExprVisitor* visitor) override
  {
    visitor->visitVariableExpr(this);
  }
};

class AssignExpr : public Expr
{
public:
  Token name;
  std::shared_ptr<Expr> value;

  AssignExpr(Token n, std::shared_ptr<Expr> v) : name(n), value(std::move(v)) {}

  void accept(ExprVisitor* visitor) override
  {
    visitor->visitAssignExpr(this);
  }
};

class LogicalExpr : public Expr
{
public:
  std::shared_ptr<Expr> left;
  Token op;
  std::shared_ptr<Expr> right;

  LogicalExpr(std::shared_ptr<Expr> l, Token o, std::shared_ptr<Expr> r)
      : left(std::move(l)), op(o), right(std::move(r))
  {}

  void accept(ExprVisitor* visitor) override
  {
    visitor->visitLogicalExpr(this);
  }
};

class CallExpr : public Expr
{
public:
  std::shared_ptr<Expr> callee;
  Token paren;
  std::vector<std::shared_ptr<Expr>> arguments;

  CallExpr(std::shared_ptr<Expr> c, Token p, std::vector<std::shared_ptr<Expr>> args)
      : callee(std::move(c)), paren(p), arguments(std::move(args))
  {}

  void accept(ExprVisitor* visitor) override
  {
    visitor->visitCallExpr(this);
  }
};

class GetExpr : public Expr
{
public:
  std::shared_ptr<Expr> object;
  Token name;

  GetExpr(std::shared_ptr<Expr> obj, Token n) : object(std::move(obj)), name(n) {}

  void accept(ExprVisitor* visitor) override
  {
    visitor->visitGetExpr(this);
  }
};

class SetExpr : public Expr
{
public:
  std::shared_ptr<Expr> object;
  Token name;
  std::shared_ptr<Expr> value;

  SetExpr(std::shared_ptr<Expr> obj, Token n, std::shared_ptr<Expr> v)
      : object(std::move(obj)), name(n), value(std::move(v))
  {}

  void accept(ExprVisitor* visitor) override
  {
    visitor->visitSetExpr(this);
  }
};

class ThisExpr : public Expr
{
public:
  Token keyword;

  explicit ThisExpr(Token k) : keyword(k) {}

  void accept(ExprVisitor* visitor) override
  {
    visitor->visitThisExpr(this);
  }
};

class TemplateExpr : public Expr
{
public:
  std::vector<std::shared_ptr<Expr>> parts;

  explicit TemplateExpr(std::vector<std::shared_ptr<Expr>> p) : parts(std::move(p)) {}

  void accept(ExprVisitor* visitor) override
  {
    visitor->visitTemplateExpr(this);
  }
};

class ArrayExpr : public Expr
{
public:
  std::vector<std::shared_ptr<Expr>> elements;

  explicit ArrayExpr(std::vector<std::shared_ptr<Expr>> e) : elements(std::move(e)) {}

  void accept(ExprVisitor* visitor) override
  {
    visitor->visitArrayExpr(this);
  }
};

class ObjectExpr : public Expr
{
public:
  std::map<std::string, std::shared_ptr<Expr>> properties;

  explicit ObjectExpr(std::map<std::string, std::shared_ptr<Expr>> props)
      : properties(std::move(props))
  {}

  void accept(ExprVisitor* visitor) override
  {
    visitor->visitObjectExpr(this);
  }
};

class TernaryExpr : public Expr
{
public:
  std::shared_ptr<Expr> condition;
  std::shared_ptr<Expr> thenBranch;
  std::shared_ptr<Expr> elseBranch;

  TernaryExpr(std::shared_ptr<Expr> cond, std::shared_ptr<Expr> thenB, std::shared_ptr<Expr> elseB)
      : condition(std::move(cond)), thenBranch(std::move(thenB)), elseBranch(std::move(elseB))
  {}

  void accept(ExprVisitor* visitor) override
  {
    visitor->visitTernaryExpr(this);
  }
};

class AwaitExpr : public Expr
{
public:
  std::shared_ptr<Expr> expr;

  explicit AwaitExpr(std::shared_ptr<Expr> e) : expr(std::move(e)) {}

  void accept(ExprVisitor* visitor) override
  {
    visitor->visitAwaitExpr(this);
  }
};

class FunctionExpr : public Expr
{
public:
  std::vector<Token> parameters;
  std::vector<std::shared_ptr<Stmt>> body;
  std::shared_ptr<Token> returnType;
  bool isAsync;

  FunctionExpr(std::vector<Token> params,
               std::vector<std::shared_ptr<Stmt>> b,
               std::shared_ptr<Token> retType,
               bool async = false)
      : parameters(std::move(params)), body(std::move(b)), returnType(std::move(retType)), isAsync(async)
  {}

  void accept(ExprVisitor* visitor) override
  {
    visitor->visitFunctionExpr(this);
  }
};

/**
 * @brief Visitor interface for statement nodes.
 */
class StmtVisitor
{
public:
  virtual ~StmtVisitor() = default;
  virtual void visitExpressionStmt(class ExpressionStmt* stmt) = 0;
  virtual void visitPrintStmt(class PrintStmt* stmt) = 0;
  virtual void visitVarStmt(class VarStmt* stmt) = 0;
  virtual void visitBlockStmt(class BlockStmt* stmt) = 0;
  virtual void visitIfStmt(class IfStmt* stmt) = 0;
  virtual void visitWhileStmt(class WhileStmt* stmt) = 0;
  virtual void visitForStmt(class ForStmt* stmt) = 0;
  virtual void visitReturnStmt(class ReturnStmt* stmt) = 0;
  virtual void visitBreakStmt(class BreakStmt* stmt) = 0;
  virtual void visitFunctionStmt(class FunctionStmt* stmt) = 0;
  virtual void visitClassStmt(class ClassStmt* stmt) = 0;
  virtual void visitTryStmt(class TryStmt* stmt) = 0;
  virtual void visitPackageStmt(class PackageStmt* stmt) = 0;
  virtual void visitExportStmt(class ExportStmt* stmt) = 0;
  virtual void visitImportStmt(class ImportStmt* stmt) = 0;
};

/**
 * @brief Base class for statement AST nodes.
 */
class Stmt
{
public:
  virtual ~Stmt() = default;
  virtual void accept(StmtVisitor* visitor) = 0;
};

class ExpressionStmt : public Stmt
{
public:
  std::shared_ptr<Expr> expression;

  explicit ExpressionStmt(std::shared_ptr<Expr> expr) : expression(std::move(expr)) {}

  void accept(StmtVisitor* visitor) override
  {
    visitor->visitExpressionStmt(this);
  }
};

class PrintStmt : public Stmt
{
public:
  std::shared_ptr<Expr> expression;

  explicit PrintStmt(std::shared_ptr<Expr> expr) : expression(std::move(expr)) {}

  void accept(StmtVisitor* visitor) override
  {
    visitor->visitPrintStmt(this);
  }
};

class VarStmt : public Stmt
{
public:
  Token name;
  std::shared_ptr<Expr> initializer;
  bool isConst;
  std::shared_ptr<Token> typeAnnotation;

  VarStmt(Token n, std::shared_ptr<Expr> init, bool isConst_, std::shared_ptr<Token> type)
      : name(std::move(n))
      , initializer(std::move(init))
      , isConst(isConst_)
      , typeAnnotation(std::move(type))
  {}

  void accept(StmtVisitor* visitor) override
  {
    visitor->visitVarStmt(this);
  }
};

class BlockStmt : public Stmt
{
public:
  std::vector<std::shared_ptr<Stmt>> statements;

  explicit BlockStmt(std::vector<std::shared_ptr<Stmt>> stmts) : statements(std::move(stmts)) {}

  void accept(StmtVisitor* visitor) override
  {
    visitor->visitBlockStmt(this);
  }
};

class IfStmt : public Stmt
{
public:
  std::shared_ptr<Expr> condition;
  std::shared_ptr<Stmt> thenBranch;
  std::shared_ptr<Stmt> elseBranch;

  IfStmt(std::shared_ptr<Expr> cond, std::shared_ptr<Stmt> thenB, std::shared_ptr<Stmt> elseB)
      : condition(std::move(cond)), thenBranch(std::move(thenB)), elseBranch(std::move(elseB))
  {}

  void accept(StmtVisitor* visitor) override
  {
    visitor->visitIfStmt(this);
  }
};

class WhileStmt : public Stmt
{
public:
  std::shared_ptr<Expr> condition;
  std::shared_ptr<Stmt> body;

  WhileStmt(std::shared_ptr<Expr> cond, std::shared_ptr<Stmt> b)
      : condition(std::move(cond)), body(std::move(b))
  {}

  void accept(StmtVisitor* visitor) override
  {
    visitor->visitWhileStmt(this);
  }
};

class ForStmt : public Stmt
{
public:
  std::shared_ptr<Stmt> initializer;
  std::shared_ptr<Expr> condition;
  std::shared_ptr<Expr> increment;
  std::shared_ptr<Stmt> body;

  ForStmt(std::shared_ptr<Stmt> init,
          std::shared_ptr<Expr> cond,
          std::shared_ptr<Expr> inc,
          std::shared_ptr<Stmt> b)
      : initializer(std::move(init))
      , condition(std::move(cond))
      , increment(std::move(inc))
      , body(std::move(b))
  {}

  void accept(StmtVisitor* visitor) override
  {
    visitor->visitForStmt(this);
  }
};

class ReturnStmt : public Stmt
{
public:
  Token keyword;
  std::shared_ptr<Expr> value;

  ReturnStmt(Token k, std::shared_ptr<Expr> val) : keyword(k), value(std::move(val)) {}

  void accept(StmtVisitor* visitor) override
  {
    visitor->visitReturnStmt(this);
  }
};

class BreakStmt : public Stmt
{
public:
  Token keyword;

  explicit BreakStmt(Token k) : keyword(k) {}

  void accept(StmtVisitor* visitor) override
  {
    visitor->visitBreakStmt(this);
  }
};

class FunctionStmt : public Stmt
{
public:
  Token name;
  std::vector<Token> parameters;
  std::vector<std::shared_ptr<Stmt>> body;
  std::shared_ptr<Token> returnType;

  bool isStatic;
  bool isGetter;
  bool isSetter;
  bool isAsync;

  FunctionStmt(Token n,
               std::vector<Token> params,
               std::vector<std::shared_ptr<Stmt>> b,
               std::shared_ptr<Token> retType,
               bool isStat = false,
               bool isGet = false,
               bool isSet = false,
               bool async = false)
      : name(n)
      , parameters(params)
      , body(std::move(b))
      , returnType(std::move(retType))
      , isStatic(isStat)
      , isGetter(isGet)
      , isSetter(isSet)
      , isAsync(async)
  {}

  void accept(StmtVisitor* visitor) override
  {
    visitor->visitFunctionStmt(this);
  }
};

class ClassStmt : public Stmt
{
public:
  Token name;
  std::shared_ptr<VariableExpr> superclass;
  std::vector<std::shared_ptr<FunctionStmt>> methods;

  ClassStmt(Token n,
            std::shared_ptr<VariableExpr> super,
            std::vector<std::shared_ptr<FunctionStmt>> m)
      : name(n), superclass(std::move(super)), methods(std::move(m))
  {}

  void accept(StmtVisitor* visitor) override
  {
    visitor->visitClassStmt(this);
  }
};

class TryStmt : public Stmt
{
public:
  std::shared_ptr<BlockStmt> tryBlock;
  Token exceptionVar;
  std::shared_ptr<BlockStmt> catchBlock;

  TryStmt(std::shared_ptr<BlockStmt> tryB, Token excVar, std::shared_ptr<BlockStmt> catchB)
      : tryBlock(std::move(tryB)), exceptionVar(excVar), catchBlock(std::move(catchB))
  {}

  void accept(StmtVisitor* visitor) override
  {
    visitor->visitTryStmt(this);
  }
};

class PackageStmt : public Stmt
{
public:
  Token name;

  explicit PackageStmt(Token n) : name(n) {}

  void accept(StmtVisitor* visitor) override
  {
    visitor->visitPackageStmt(this);
  }
};

class ImportStmt : public Stmt
{
public:
  std::string modulePath;
  std::vector<Token> names;
  std::map<std::string, std::string> aliases;
  bool importAll;
  bool isNamespaceImport;

  ImportStmt(std::string path,
             std::vector<Token> n,
             std::map<std::string, std::string> a,
             bool all,
             bool ns = false)
      : modulePath(std::move(path))
      , names(std::move(n))
      , aliases(std::move(a))
      , importAll(all)
      , isNamespaceImport(ns)
  {}

  void accept(StmtVisitor* visitor) override
  {
    visitor->visitImportStmt(this);
  }
};

class ExportStmt : public Stmt
{
public:
  std::vector<Token> names;

  explicit ExportStmt(std::vector<Token> n) : names(n) {}

  void accept(StmtVisitor* visitor) override
  {
    visitor->visitExportStmt(this);
  }
};

#endif // NODES_H
