#include "Parser.h"

#include "../utils/Logger.h"

Parser::Parser(const std::vector<Token>& tokens_) : tokens(tokens_) {}

std::vector<std::shared_ptr<Stmt>> Parser::parse()
{
  std::vector<std::shared_ptr<Stmt>> statements;

  while (!isAtEnd()) {
    auto decl = declaration();
    if (!decl.empty()) {
      statements.insert(statements.end(),
                        std::make_move_iterator(decl.begin()),
                        std::make_move_iterator(decl.end()));
    }
  }

  return statements;
}

std::vector<std::shared_ptr<Stmt>> Parser::declaration()
{
  std::vector<std::shared_ptr<Stmt>> result;

  if (match(TokenType::PACKAGE)) {
    result.push_back(packageDeclaration());
  }
  else if (match(TokenType::IMPORT)) {
    result.push_back(importDeclaration());
  }
  else if (match(TokenType::ASYNC)) {
    consume(TokenType::FUNC, "Expect 'func' after 'async'.");
    result.push_back(functionDeclaration(previous(), true));
  }
  else if (match(TokenType::FUNC)) {
    result.push_back(functionDeclaration(previous(), false));
  }
  else if (match(TokenType::CLASS)) {
    result.push_back(classDeclaration());
  }
  else if (match(TokenType::VAR) || match(TokenType::CONST)) {
    result.push_back(variableDeclaration());
  }
  else if (match(TokenType::EXPORT)) {
    result.push_back(exportDeclaration());
  }
  else {
    result.push_back(statement());
  }

  return result;
}

std::shared_ptr<Stmt> Parser::importDeclaration()
{
  std::vector<Token> names;
  std::map<std::string, std::string> aliases;
  bool importAll = false;
  bool isNamespaceImport = false;

  if (check(TokenType::STRING)) {
    importAll = true;
  }
  // import * from "module"
  else if (match(TokenType::MULTIPLY)) {
    importAll = true;
    consume(TokenType::FROM, "Expect 'from' after '*'.");
  }
  else {
    if (match(TokenType::LEFT_BRACE)) {
      do {
        Token name = consume(TokenType::IDENTIFIER, "Expect identifier in import list.");
        names.push_back(name);
        std::string alias = name.lexeme; // default alias is the name itself
        if (match(TokenType::AS)) {
          Token aliasToken = consume(TokenType::IDENTIFIER, "Expect alias after 'as'.");
          alias = aliasToken.lexeme;
        }
        aliases[name.lexeme] = alias;
        if (!match(TokenType::COMMA)) {
          break;
        }
      } while (true);
      consume(TokenType::RIGHT_BRACE, "Expect '}' after import list.");
    }
    else {
      Token name = consume(TokenType::IDENTIFIER, "Expect module name or identifier.");
      names.push_back(name);
      aliases[name.lexeme] = name.lexeme;

      if (match(TokenType::SEMICOLON)) {
        // Special case: import module; -> import * from "module";
        return std::make_shared<ImportStmt>(name.lexeme,
                                            std::vector<Token>(),
                                            std::map<std::string, std::string>(),
                                            true,
                                            false);
      }

      isNamespaceImport = true;
    }
    consume(TokenType::FROM, "Expect 'from' after import specifiers.");
  }

  Token pathToken = consume(TokenType::STRING, "Expect module path string.");
  std::string modulePath = pathToken.lexeme.substr(1, pathToken.lexeme.size() - 2);

  consume(TokenType::SEMICOLON, "Expect ';' after import declaration.");

  return std::make_shared<ImportStmt>(modulePath,
                                      std::move(names),
                                      std::move(aliases),
                                      importAll,
                                      isNamespaceImport);
}

std::shared_ptr<Stmt> Parser::statement()
{
  if (match(TokenType::IF)) {
    return ifStatement();
  }
  if (match(TokenType::WHILE)) {
    return whileStatement();
  }
  if (match(TokenType::FOR)) {
    return forStatement();
  }
  if (match(TokenType::LEFT_BRACE)) {
    return blockStatement();
  }
  if (match(TokenType::RETURN)) {
    return returnStatement();
  }
  if (match(TokenType::BREAK)) {
    return breakStatement();
  }
  if (match(TokenType::TRY)) {
    return tryStatement();
  }
  if (match(TokenType::PRINT)) {
    return printStatement();
  }

  return expressionStatement();
}

std::shared_ptr<Stmt> Parser::packageDeclaration()
{
  Token packageName = consume(TokenType::IDENTIFIER, "Expect package name.");
  consume(TokenType::SEMICOLON, "Expect ';' after package declaration.");
  return std::make_shared<PackageStmt>(packageName);
}

std::shared_ptr<Stmt> Parser::variableDeclaration()
{
  bool isConst = previous().type == TokenType::CONST;
  Token name = consume(TokenType::IDENTIFIER, "Expect variable name.");

  std::shared_ptr<Token> typeAnnotation = nullptr;
  if (match(TokenType::COLON)) {
    Token type = consume(TokenType::IDENTIFIER, "Expect type annotation.");
    typeAnnotation = std::make_shared<Token>(type);
  }

  std::shared_ptr<Expr> initializer = nullptr;
  if (match(TokenType::ASSIGN)) {
    initializer = expression();
  }

  consume(TokenType::SEMICOLON, "Expect ';' after variable declaration.");
  return std::make_shared<VarStmt>(name,
                                   std::move(initializer),
                                   isConst,
                                   std::move(typeAnnotation));
}

std::shared_ptr<Stmt> Parser::functionDeclaration(const Token& kind, bool isAsync)
{
  (void)kind;

  Token name = consume(TokenType::IDENTIFIER, "Expect function name.");
  consume(TokenType::LEFT_PAREN, "Expect '(' after function name.");

  std::vector<Token> parameters;
  if (!check(TokenType::RIGHT_PAREN)) {
    do {
      parameters.push_back(consume(TokenType::IDENTIFIER, "Expect parameter name."));

      if (match(TokenType::COLON)) {
        consume(TokenType::IDENTIFIER, "Expect parameter type.");
      }

      if (!match(TokenType::COMMA)) {
        break;
      }
    } while (true);
  }

  consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");

  std::shared_ptr<Token> returnType = nullptr;

  // Swift style: func foo() -> int {}
  if (match(TokenType::ARROW)) {
    Token type = consume(TokenType::IDENTIFIER, "Expect return type.");
    returnType = std::make_shared<Token>(type);
  }
  // C style: func foo() int {}
  else if (check(TokenType::IDENTIFIER) && !check(TokenType::LEFT_BRACE)) {
    returnType = std::make_shared<Token>(advance());
  }

  consume(TokenType::LEFT_BRACE, "Expect '{' before function body.");

  auto block = blockStatement();
  auto* blockStmt = static_cast<BlockStmt*>(block.get());

  std::vector<std::shared_ptr<Stmt>> body;
  if (blockStmt) {
    for (auto& stmt : blockStmt->statements) {
      body.push_back(std::move(stmt));
    }
  }

  return std::make_shared<FunctionStmt>(name,
                                        std::move(parameters),
                                        std::move(body),
                                        std::move(returnType),
                                        false, false, false,
                                        isAsync);
}

std::shared_ptr<Stmt> Parser::classDeclaration()
{
  Token name = consume(TokenType::IDENTIFIER, "Expect class name.");

  std::shared_ptr<VariableExpr> superclass = nullptr;
  if (match(TokenType::LESS)) {
    consume(TokenType::IDENTIFIER, "Expect superclass name.");
    superclass = std::make_shared<VariableExpr>(previous());
  }

  consume(TokenType::LEFT_BRACE, "Expect '{' before class body.");

  std::vector<std::shared_ptr<FunctionStmt>> methods;

  while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
    bool isStatic = match(TokenType::STATIC);
    bool isGet = match(TokenType::GET);
    bool isSet = match(TokenType::SET);

    Token methodName = check(TokenType::CONSTRUCTOR)
                           ? (consume(TokenType::CONSTRUCTOR, "Expect constructor."), previous())
                           : consume(TokenType::IDENTIFIER, "Expect method name.");

    methods.push_back(methodDeclaration(methodName, isStatic, isGet, isSet));
  }

  consume(TokenType::RIGHT_BRACE, "Expect '}' after class body.");

  return std::make_shared<ClassStmt>(name, std::move(superclass), std::move(methods));
}

std::shared_ptr<FunctionStmt>
Parser::methodDeclaration(Token name, bool isStatic, bool isGet, bool isSet)
{
  std::vector<Token> parameters;

  consume(TokenType::LEFT_PAREN, "Expect '(' after method name.");
  if (!check(TokenType::RIGHT_PAREN)) {
    do {
      parameters.push_back(consume(TokenType::IDENTIFIER, "Expect parameter name."));
    } while (match(TokenType::COMMA));
  }
  consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");

  consume(TokenType::LEFT_BRACE, "Expect '{' before method body.");

  auto block = blockStatement();
  auto* blockStmt = static_cast<BlockStmt*>(block.get());

  std::vector<std::shared_ptr<Stmt>> body;
  if (blockStmt) {
    for (auto& stmt : blockStmt->statements) {
      body.push_back(std::move(stmt));
    }
  }

  return std::make_shared<FunctionStmt>(name,
                                        std::move(parameters),
                                        std::move(body),
                                        nullptr,
                                        isStatic,
                                        isGet,
                                        isSet);
}

std::shared_ptr<Stmt> Parser::blockStatement()
{
  std::vector<std::shared_ptr<Stmt>> statements;
  while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
    auto decl = declaration();
    if (!decl.empty()) {
      statements.push_back(std::move(decl[0]));
    }
  }

  consume(TokenType::RIGHT_BRACE, "Expect '}' after block.");
  return std::make_shared<BlockStmt>(std::move(statements));
}

std::shared_ptr<Stmt> Parser::ifStatement()
{
  consume(TokenType::LEFT_PAREN, "Expect '(' after 'if'.");
  auto condition = expression();
  consume(TokenType::RIGHT_PAREN, "Expect ')' after if condition.");

  auto thenBranch = statement();
  std::shared_ptr<Stmt> elseBranch = nullptr;

  if (match(TokenType::ELSE)) {
    elseBranch = statement();
  }

  return std::make_shared<IfStmt>(std::move(condition),
                                  std::move(thenBranch),
                                  std::move(elseBranch));
}

std::shared_ptr<Stmt> Parser::whileStatement()
{
  consume(TokenType::LEFT_PAREN, "Expect '(' after 'while'.");
  auto condition = expression();
  consume(TokenType::RIGHT_PAREN, "Expect ')' after while condition.");

  auto body = statement();

  return std::make_shared<WhileStmt>(std::move(condition), std::move(body));
}

std::shared_ptr<Stmt> Parser::forStatement()
{
  consume(TokenType::LEFT_PAREN, "Expect '(' after 'for'.");

  std::shared_ptr<Stmt> initializer;
  if (match(TokenType::SEMICOLON)) {
    initializer = nullptr;
  }
  else if (match(TokenType::VAR) || match(TokenType::CONST)) {
    initializer = variableDeclaration();
  }
  else {
    initializer = expressionStatement();
  }

  std::shared_ptr<Expr> condition;
  if (!check(TokenType::SEMICOLON)) {
    condition = expression();
  }
  consume(TokenType::SEMICOLON, "Expect ';' after loop condition.");

  std::shared_ptr<Expr> increment;
  if (!check(TokenType::RIGHT_PAREN)) {
    increment = expression();
  }
  consume(TokenType::RIGHT_PAREN, "Expect ')' after for clauses.");

  auto body = statement();

  return std::make_shared<ForStmt>(std::move(initializer),
                                   std::move(condition),
                                   std::move(increment),
                                   std::move(body));
}

std::shared_ptr<Stmt> Parser::tryStatement()
{
  consume(TokenType::LEFT_BRACE, "Expect '{' after 'try'.");
  auto tryBlockRaw = blockStatement();
  auto tryBlock = std::static_pointer_cast<BlockStmt>(tryBlockRaw);

  consume(TokenType::CATCH, "Expect 'catch' after try block.");
  consume(TokenType::LEFT_PAREN, "Expect '(' after 'catch'.");
  Token exceptionVar = consume(TokenType::IDENTIFIER, "Expect exception variable name.");
  consume(TokenType::RIGHT_PAREN, "Expect ')' after exception variable.");

  consume(TokenType::LEFT_BRACE, "Expect '{' after catch clause.");
  auto catchBlockRaw = blockStatement();
  auto catchBlock = std::static_pointer_cast<BlockStmt>(catchBlockRaw);

  return std::make_shared<TryStmt>(std::move(tryBlock), exceptionVar, std::move(catchBlock));
}

std::shared_ptr<Stmt> Parser::returnStatement()
{
  Token keyword = previous();
  std::shared_ptr<Expr> value = nullptr;

  if (!check(TokenType::SEMICOLON)) {
    value = expression();
  }
  consume(TokenType::SEMICOLON, "Expect ';' after return value.");
  return std::make_shared<ReturnStmt>(keyword, std::move(value));
}

std::shared_ptr<Stmt> Parser::breakStatement()
{
  Token keyword = previous();
  consume(TokenType::SEMICOLON, "Expect ';' after break.");
  return std::make_shared<BreakStmt>(keyword);
}

std::shared_ptr<Stmt> Parser::expressionStatement()
{
  auto expr = expression();
  consume(TokenType::SEMICOLON, "Expect ';' after expression.");
  return std::make_shared<ExpressionStmt>(std::move(expr));
}

std::shared_ptr<Stmt> Parser::printStatement()
{
  consume(TokenType::LEFT_PAREN, "Expect '(' after 'print'.");
  auto value = expression();
  consume(TokenType::RIGHT_PAREN, "Expect ')' after value.");
  consume(TokenType::SEMICOLON, "Expect ';' after ')'.");
  return std::make_shared<PrintStmt>(std::move(value));
}

std::shared_ptr<Stmt> Parser::exportDeclaration()
{
  std::vector<Token> names;

  if (match(TokenType::LEFT_BRACE)) {
    do {
      names.push_back(consume(TokenType::IDENTIFIER, "Expect identifier in export list."));
      if (!match(TokenType::COMMA)) {
        break;
      }
    } while (true);
    consume(TokenType::RIGHT_BRACE, "Expect '}' after export list.");
  }
  else {
    names.push_back(consume(TokenType::IDENTIFIER, "Expect identifier to export."));
  }

  consume(TokenType::SEMICOLON, "Expect ';' after export declaration.");
  return std::make_shared<ExportStmt>(names);
}

std::shared_ptr<Expr> Parser::expression()
{
  return assignment();
}

std::shared_ptr<Expr> Parser::assignment()
{
  auto expr = ternary();

  if (match(TokenType::ASSIGN)) {
    Token equals = previous();
    auto value = assignment();

    if (auto* varExpr = dynamic_cast<VariableExpr*>(expr.get())) {
      return std::make_shared<AssignExpr>(varExpr->name, std::move(value));
    }

    if (auto* getExpr = dynamic_cast<GetExpr*>(expr.get())) {
      return std::make_shared<SetExpr>(std::move(getExpr->object), getExpr->name, std::move(value));
    }

    reportError(equals, "Invalid assignment target.");
  }
  return expr;
}

std::shared_ptr<Expr> Parser::ternary()
{
  auto expr = logicalOr();

  if (match(TokenType::QUESTION)) {
    auto thenBranch = expression();
    consume(TokenType::COLON, "Expect ':' after ternary condition.");
    auto elseBranch = ternary();
    return std::make_shared<TernaryExpr>(std::move(expr),
                                         std::move(thenBranch),
                                         std::move(elseBranch));
  }
  return expr;
}

std::shared_ptr<Expr> Parser::logicalOr()
{
  auto expr = logicalAnd();

  while (match(TokenType::OR)) {
    Token op = previous();
    auto right = logicalAnd();
    expr = std::make_shared<LogicalExpr>(std::move(expr), op, std::move(right));
  }

  return expr;
}

std::shared_ptr<Expr> Parser::logicalAnd()
{
  auto expr = equality();

  while (match(TokenType::AND)) {
    Token op = previous();
    auto right = equality();
    expr = std::make_shared<LogicalExpr>(std::move(expr), op, std::move(right));
  }

  return expr;
}

std::shared_ptr<Expr> Parser::equality()
{
  auto expr = comparison();

  while (match(TokenType::EQUAL) || match(TokenType::NOT_EQUAL)) {
    Token op = previous();
    auto right = comparison();
    expr = std::make_shared<BinaryExpr>(std::move(expr), op, std::move(right));
  }

  return expr;
}

std::shared_ptr<Expr> Parser::comparison()
{
  auto expr = term();

  while (match(TokenType::GREATER) || match(TokenType::GREATER_EQUAL) || match(TokenType::LESS)
         || match(TokenType::LESS_EQUAL)) {
    Token op = previous();
    auto right = term();
    expr = std::make_shared<BinaryExpr>(std::move(expr), op, std::move(right));
  }
  return expr;
}

std::shared_ptr<Expr> Parser::term()
{
  auto expr = factor();

  while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
    Token op = previous();
    auto right = factor();
    expr = std::make_shared<BinaryExpr>(std::move(expr), op, std::move(right));
  }

  return expr;
}

std::shared_ptr<Expr> Parser::factor()
{
  auto expr = unary();

  while (match(TokenType::MULTIPLY) || match(TokenType::DIVIDE) || match(TokenType::MODULO)) {
    Token op = previous();
    auto right = unary();
    expr = std::make_shared<BinaryExpr>(std::move(expr), op, std::move(right));
  }

  return expr;
}

std::shared_ptr<Expr> Parser::unary()
{
  if (match(TokenType::AWAIT)) {
    auto right = unary();
    return std::make_shared<AwaitExpr>(std::move(right));
  }

  if (match(TokenType::NOT) || match(TokenType::MINUS)) {
    Token op = previous();
    auto right = unary();
    return std::make_shared<UnaryExpr>(op, std::move(right));
  }

  return call();
}

std::shared_ptr<Expr> Parser::call()
{
  auto expr = primary();

  while (true) {
    if (match(TokenType::LEFT_PAREN)) {
      expr = finishCall(std::move(expr));
    }
    else if (match(TokenType::DOT)) {
      if (match(TokenType::IDENTIFIER) || match(TokenType::GET) || match(TokenType::SET)) {
        Token name = previous();
        expr = std::make_shared<GetExpr>(std::move(expr), name);
      }
      else {
        reportError("Expect property name after '.'.");
      }
    }
    else if (match(TokenType::PLUS_PLUS) || match(TokenType::MINUS_MINUS)) {
      Token op = previous();
      if (auto* varExpr = dynamic_cast<VariableExpr*>(expr.get())) {
        expr = std::make_shared<IncrementExpr>(varExpr->name, op);
      }
      else if (auto* getExpr = dynamic_cast<GetExpr*>(expr.get())) {
        expr = std::make_shared<IncrementExpr>(getExpr->name, op);
      }
      else {
        reportError(op, "Invalid increment/decrement target.");
      }
    }
    else {
      break;
    }
  }
  return expr;
}

std::shared_ptr<Expr> Parser::finishCall(std::shared_ptr<Expr> callee)
{
  std::vector<std::shared_ptr<Expr>> arguments;

  if (!check(TokenType::RIGHT_PAREN)) {
    do {
      if (arguments.size() >= 255) {
        reportError(previous(), "Can't have more than 255 arguments.");
      }
      arguments.push_back(expression());

      if (!match(TokenType::COMMA)) {
        break;
      }
    } while (true);
  }

  Token paren = consume(TokenType::RIGHT_PAREN, "Expect ')' after arguments.");

  return std::make_shared<CallExpr>(std::move(callee), paren, std::move(arguments));
}

std::shared_ptr<Expr> Parser::primary()
{
  if (match(TokenType::FALSE)) {
    return std::make_shared<LiteralExpr>(previous());
  }
  if (match(TokenType::TRUE)) {
    return std::make_shared<LiteralExpr>(previous());
  }
  if (match(TokenType::NULL_VAL)) {
    return std::make_shared<LiteralExpr>(previous());
  }
  if (match(TokenType::STRING) || match(TokenType::NUMBER)) {
    return std::make_shared<LiteralExpr>(previous());
  }

  if (match(TokenType::BACKTICK)) {
    return templateExpr();
  }

  if (match(TokenType::ASYNC)) {
    consume(TokenType::FUNC, "Expect 'func' after 'async'.");
    return anonymousFunction(true);
  }
  if (match(TokenType::FUNC)) {
    return anonymousFunction(false);
  }

  if (match(TokenType::LEFT_PAREN)) {
    auto expr = expression();
    consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
    return std::make_shared<GroupingExpr>(std::move(expr));
  }

  if (match(TokenType::LEFT_BRACKET)) {
    std::vector<std::shared_ptr<Expr>> elements;
    if (!check(TokenType::RIGHT_BRACKET)) {
      do {
        elements.push_back(expression());
      } while (match(TokenType::COMMA));
    }
    consume(TokenType::RIGHT_BRACKET, "Expect ']' after array elements.");
    return std::make_shared<ArrayExpr>(std::move(elements));
  }

  if (match(TokenType::LEFT_BRACE)) {
    std::map<std::string, std::shared_ptr<Expr>> properties;
    if (!check(TokenType::RIGHT_BRACE)) {
      do {
        if (match(TokenType::IDENTIFIER) || match(TokenType::STRING) || match(TokenType::GET)
            || match(TokenType::SET)) {
          Token keyToken = previous();
          std::string key = keyToken.lexeme;
          if (keyToken.type == TokenType::STRING) {
            key = key.substr(1, key.size() - 2);
          }

          consume(TokenType::COLON, "Expect ':' after property name.");
          properties[key] = expression();
        }
        else {
          reportError("Expect property name in object literal.");
        }
      } while (match(TokenType::COMMA));
    }
    consume(TokenType::RIGHT_BRACE, "Expect '}' after object properties.");
    // Cast to shared_ptr<Expr> explicitly if needed, but make_shared<ObjectExpr> should work
    // if ObjectExpr is fully defined.
    return std::static_pointer_cast<Expr>(std::make_shared<ObjectExpr>(std::move(properties)));
  }

  if (match(TokenType::NEW)) {
    Token className = consume(TokenType::IDENTIFIER, "Expect class name.");
    auto callee = std::make_shared<VariableExpr>(className);

    if (match(TokenType::LEFT_PAREN)) {
      return finishCall(std::move(callee));
    }

    Token dummyToken(TokenType::RIGHT_PAREN, ")", className.line, className.column);
    std::vector<std::shared_ptr<Expr>> emptyArgs;
    return std::make_shared<CallExpr>(std::move(callee), dummyToken, std::move(emptyArgs));
  }

  if (match(TokenType::IDENTIFIER)) {
    return std::make_shared<VariableExpr>(previous());
  }

  if (match(TokenType::THIS)) {
    return std::make_shared<ThisExpr>(previous());
  }

  reportError("Expect expression.");
  return nullptr;
}

std::shared_ptr<Expr> Parser::templateExpr()
{
  std::vector<std::shared_ptr<Expr>> parts;

  while (!check(TokenType::BACKTICK) && !isAtEnd()) {
    if (match(TokenType::TEMPLATE_STRING_PART)) {
      parts.push_back(std::make_shared<LiteralExpr>(previous()));
    }
    else if (match(TokenType::DOLLAR_BRACE)) {
      parts.push_back(expression());
    }
    else {
      advance();
    }
  }

  consume(TokenType::BACKTICK, "Expect '`' after template string.");

  return std::make_shared<TemplateExpr>(std::move(parts));
}

std::shared_ptr<Expr> Parser::anonymousFunction(bool isAsync)
{
  consume(TokenType::LEFT_PAREN, "Expect '(' after 'func'.");

  std::vector<Token> parameters;
  if (!check(TokenType::RIGHT_PAREN)) {
    do {
      parameters.push_back(consume(TokenType::IDENTIFIER, "Expect parameter name."));
      if (match(TokenType::COLON)) {
        consume(TokenType::IDENTIFIER, "Expect parameter type.");
      }
      if (!match(TokenType::COMMA)) {
        break;
      }
    } while (true);
  }
  consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");

  std::shared_ptr<Token> returnType = nullptr;
  if (match(TokenType::ARROW)) {
    Token type = consume(TokenType::IDENTIFIER, "Expect return type.");
    returnType = std::make_shared<Token>(type);
  }
  else if (check(TokenType::IDENTIFIER) && !check(TokenType::LEFT_BRACE)) {
    returnType = std::make_shared<Token>(advance());
  }

  consume(TokenType::LEFT_BRACE, "Expect '{' before function body.");
  auto block = blockStatement();
  auto* blockStmt = static_cast<BlockStmt*>(block.get());

  std::vector<std::shared_ptr<Stmt>> body;
  if (blockStmt) {
    for (auto& stmt : blockStmt->statements) {
      body.push_back(std::move(stmt));
    }
  }

  return std::make_shared<FunctionExpr>(std::move(parameters),
                                        std::move(body),
                                        std::move(returnType),
                                        isAsync);
}

Token Parser::consume(TokenType type, const std::string& message)
{
  if (check(type)) {
    return advance();
  }

  reportError(message);
  return Token(TokenType::EOF_TOKEN, "", "<unknown>", 0, 0);
}

bool Parser::match(TokenType type)
{
  if (check(type)) {
    advance();
    return true;
  }
  return false;
}

bool Parser::check(TokenType type)
{
  if (isAtEnd()) {
    return false;
  }
  return tokens[current].type == type;
}

Token Parser::advance()
{
  if (!isAtEnd()) {
    current++;
  }
  return previous();
}

bool Parser::isAtEnd()
{
  return current >= tokens.size() || tokens[current].type == TokenType::EOF_TOKEN;
}

Token Parser::previous()
{
  return tokens[current - 1];
}

void Parser::synchronize()
{
  advance();

  while (!isAtEnd()) {
    if (previous().type == TokenType::SEMICOLON) {
      return;
    }

    switch (tokens[current].type) {
      case TokenType::PACKAGE:
      case TokenType::FUNC:
      case TokenType::VAR:
      case TokenType::CONST:
      case TokenType::IF:
      case TokenType::FOR:
      case TokenType::WHILE:
      case TokenType::RETURN:
      case TokenType::BREAK:
        return;
      default:; // Do nothing
    }

    advance();
  }
}

void Parser::reportError(const Token& token, const std::string& message)
{
  throw LoggerException(message, token.getLocation(), "SyntaxError");
}

void Parser::reportError(const std::string& message)
{
  if (!isAtEnd()) {
    throw LoggerException(message, tokens[current].getLocation(), "SyntaxError");
  }

  throw LoggerException(message, previous().getLocation(), "SyntaxError");
}
