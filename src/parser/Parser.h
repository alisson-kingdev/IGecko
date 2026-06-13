#ifndef PARSER_H
#define PARSER_H

#include <memory>
#include <vector>

#include "../ast/Nodes.h"
#include "../lexer/Lexer.h"

/**
 * @brief Parses the token stream into an Abstract Syntax Tree (AST).
 *
 * Uses a recursive descent parsing strategy to generate statements and
 * expressions while enforcing language grammar.
 */
class Parser
{
public:
  /** @brief Initializes the parser with a sequence of tokens. */
  explicit Parser(const std::vector<Token>& tokens_);

  /** @brief Orchestrates the parsing process. */
  std::vector<std::shared_ptr<Stmt>> parse();

private:
  const std::vector<Token>& tokens;
  size_t current = 0;

  // --- Statement parsing ---
  std::vector<std::shared_ptr<Stmt>> declaration();
  std::shared_ptr<Stmt> statement();
  std::shared_ptr<Stmt> blockStatement();
  std::shared_ptr<Stmt> ifStatement();
  std::shared_ptr<Stmt> whileStatement();
  std::shared_ptr<Stmt> forStatement();
  std::shared_ptr<Stmt> returnStatement();
  std::shared_ptr<Stmt> breakStatement();
  std::shared_ptr<Stmt> functionDeclaration(const Token& kind, bool isAsync = false);
  std::shared_ptr<Stmt> classDeclaration();
  std::shared_ptr<Stmt> tryStatement();
  std::shared_ptr<Stmt> expressionStatement();
  std::shared_ptr<Stmt> printStatement();
  std::shared_ptr<Stmt> variableDeclaration();
  std::shared_ptr<Stmt> packageDeclaration();
  std::shared_ptr<Stmt> exportDeclaration();
  std::shared_ptr<Stmt> importDeclaration();

  // --- Expression parsing ---
  std::shared_ptr<Expr> expression();
  std::shared_ptr<Expr> assignment();
  std::shared_ptr<Expr> ternary();
  std::shared_ptr<Expr> logicalOr();
  std::shared_ptr<Expr> logicalAnd();
  std::shared_ptr<Expr> equality();
  std::shared_ptr<Expr> comparison();
  std::shared_ptr<Expr> term();
  std::shared_ptr<Expr> factor();
  std::shared_ptr<Expr> unary();
  std::shared_ptr<Expr> call();
  std::shared_ptr<Expr> primary();
  std::shared_ptr<Expr> templateExpr();
  std::shared_ptr<Expr> anonymousFunction(bool isAsync = false);

  std::shared_ptr<FunctionStmt>
  methodDeclaration(Token name, bool isStatic, bool isGet, bool isSet);
  std::shared_ptr<Expr> finishCall(std::shared_ptr<Expr> callee);

  // --- Helpers ---
  std::vector<Token> functionParameters();
  std::shared_ptr<Token> typeAnnotation();

  Token consume(TokenType type, const std::string& message);
  bool match(TokenType type);
  bool check(TokenType type);
  Token advance();
  bool isAtEnd();
  Token previous();
  void synchronize();

  void reportError(const Token& token, const std::string& message);
  void reportError(const std::string& message);

  template<typename T>
  std::shared_ptr<T> dynamicCast(std::shared_ptr<Stmt> ptr);
};

#endif // PARSER_H
