#include "Lexer.h"

#include <cctype>
#include <stdexcept>

#include "../utils/Logger.h"

Lexer::Lexer(const std::string& source_, const std::string& filename_)
    : source(source_), filename(filename_)
{
  Logger::setCurrentFile(filename);
  stateStack.push_back(LexerState::NORMAL);
}

void Lexer::reportError(const std::string& message)
{
  throw LoggerException(message, SourceLocation(filename, line, column), "LexicalError");
}

std::vector<Token> Lexer::tokenize()
{
  while (!isAtEnd()) {
    start = current;
    column = static_cast<int>(current - lastNewline);

    if (stateStack.back() == LexerState::TEMPLATE) {
      templateString();
    }
    else {
      scanToken();
    }
  }

  tokens.push_back(Token(TokenType::EOF_TOKEN, "", line, column));
  return tokens;
}

void Lexer::scanToken()
{
  char c = advance();
  switch (c) {
    case '(':
      addToken(TokenType::LEFT_PAREN);
      break;
    case ')':
      addToken(TokenType::RIGHT_PAREN);
      break;
    case '{':
      if (!braceStack.empty()) {
        braceStack.back()++;
      }
      addToken(TokenType::LEFT_BRACE);
      break;
    case '}':
      if (!braceStack.empty()) {
        if (braceStack.back() == 0) {
          braceStack.pop_back();
          stateStack.pop_back();
          return;
        }
        else {
          braceStack.back()--;
        }
      }
      addToken(TokenType::RIGHT_BRACE);
      break;
    case '`':
      addToken(TokenType::BACKTICK);
      stateStack.push_back(LexerState::TEMPLATE);
      break;

    case '[':
      addToken(TokenType::LEFT_BRACKET);
      break;
    case ']':
      addToken(TokenType::RIGHT_BRACKET);
      break;
    case ';':
      addToken(TokenType::SEMICOLON);
      break;
    case ':':
      addToken(TokenType::COLON);
      break;
    case '?':
      addToken(TokenType::QUESTION);
      break;
    case ',':
      addToken(TokenType::COMMA);
      break;
    case '.':
      addToken(TokenType::DOT);
      break;
    case '-':
      if (match('>')) {
        addToken(TokenType::ARROW);
      }
      else if (match('-')) {
        addToken(TokenType::MINUS_MINUS);
      }
      else {
        addToken(TokenType::MINUS);
      }
      break;
    case '+':
      if (match('+')) {
        addToken(TokenType::PLUS_PLUS);
      }
      else {
        addToken(TokenType::PLUS);
      }
      break;
    case '/':
      if (match('/')) {
        while (peek() != '\n' && !isAtEnd()) {
          advance();
        }
      }
      else if (match('*')) {
        bool closed = false;
        while (!isAtEnd()) {
          if (peek() == '\n') {
            line++;
            lastNewline = current;
          }
          if (peek() == '*' && peekNext() == '/') {
            advance();
            advance();
            closed = true;
            break;
          }
          advance();
        }
        if (!closed) {
          reportError("Unterminated block comment.");
        }
      }
      else {
        addToken(TokenType::DIVIDE);
      }
      break;
    case '*':
      addToken(TokenType::MULTIPLY);
      break;
    case '%':
      addToken(TokenType::MODULO);
      break;
    case '!':
      addToken(match('=') ? TokenType::NOT_EQUAL : TokenType::NOT);
      break;
    case '=':
      addToken(match('=') ? TokenType::EQUAL : TokenType::ASSIGN);
      break;
    case '<':
      addToken(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS);
      break;
    case '>':
      addToken(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER);
      break;
    case '&':
      if (match('&')) {
        addToken(TokenType::AND);
      }
      else {
        reportError("Unexpected character: &");
      }
      break;
    case '|':
      if (match('|')) {
        addToken(TokenType::OR);
      }
      else {
        reportError("Unexpected character: |");
      }
      break;

    case ' ':
    case '\r':
    case '\t':
      break;

    case '\n':
      line++;
      lastNewline = current;
      column = 1;
      break;

    case '"':
      string();
      break;
    case '\'':
      singleQuoteString();
      break;

    default:
      if (isDigit(c)) {
        number();
      }
      else if (isAlpha(c)) {
        identifier();
      }
      else {
        reportError("Unexpected character: " + std::string(1, c));
      }
      break;
  }
}

void Lexer::string()
{
  while (peek() != '"' && !isAtEnd()) {
    if (peek() == '\\') {
      advance();
      if (!isAtEnd()) {
        advance();
      }
      continue;
    }
    if (peek() == '\n') {
      line++;
      lastNewline = current;
    }
    advance();
  }

  if (isAtEnd()) {
    reportError("Unterminated string.");
    return;
  }

  advance();

  std::string value = source.substr(start + 1, current - start - 2);
  addToken(TokenType::STRING);
}

void Lexer::singleQuoteString()
{
  while (peek() != '\'' && !isAtEnd()) {
    if (peek() == '\\') {
      advance();
      if (!isAtEnd()) {
        advance();
      }
      continue;
    }
    if (peek() == '\n') {
      line++;
      lastNewline = current;
    }
    advance();
  }

  if (isAtEnd()) {
    reportError("Unterminated string.");
    return;
  }

  advance();

  std::string value = source.substr(start + 1, current - start - 2);
  addToken(TokenType::STRING);
}

void Lexer::number()
{
  while (isDigit(peek())) {
    advance();
  }

  if (peek() == '.' && isDigit(peekNext())) {
    advance();
    while (isDigit(peek())) {
      advance();
    }
  }

  addToken(TokenType::NUMBER);
}

void Lexer::identifier()
{
  while (isAlphaNumeric(peek())) {
    advance();
  }

  std::string text = source.substr(start, current - start);

  TokenType type = TokenType::IDENTIFIER;
  if (text == "package") {
    type = TokenType::PACKAGE;
  }
  else if (text == "import") {
    type = TokenType::IMPORT;
  }
  else if (text == "export") {
    type = TokenType::EXPORT;
  }
  else if (text == "from") {
    type = TokenType::FROM;
  }
  else if (text == "as") {
    type = TokenType::AS;
  }
  else if (text == "func") {
    type = TokenType::FUNC;
  }
  else if (text == "var") {
    type = TokenType::VAR;
  }
  else if (text == "const") {
    type = TokenType::CONST;
  }
  else if (text == "if") {
    type = TokenType::IF;
  }
  else if (text == "else") {
    type = TokenType::ELSE;
  }
  else if (text == "for") {
    type = TokenType::FOR;
  }
  else if (text == "while") {
    type = TokenType::WHILE;
  }
  else if (text == "return") {
    type = TokenType::RETURN;
  }
  else if (text == "class") {
    type = TokenType::CLASS;
  }
  else if (text == "constructor") {
    type = TokenType::CONSTRUCTOR;
  }
  else if (text == "static") {
    type = TokenType::STATIC;
  }
  else if (text == "get") {
    type = TokenType::GET;
  }
  else if (text == "set") {
    type = TokenType::SET;
  }
  else if (text == "new") {
    type = TokenType::NEW;
  }
  else if (text == "async") {
    type = TokenType::ASYNC;
  }
  else if (text == "await") {
    type = TokenType::AWAIT;
  }
  else if (text == "this") {
    type = TokenType::THIS;
  }
  else if (text == "try") {
    type = TokenType::TRY;
  }
  else if (text == "catch") {
    type = TokenType::CATCH;
  }
  else if (text == "true") {
    type = TokenType::TRUE;
  }
  else if (text == "false") {
    type = TokenType::FALSE;
  }
  else if (text == "null") {
    type = TokenType::NULL_VAL;
  }
  else if (text == "print") {
    type = TokenType::PRINT;
  }
  else if (text == "break") {
    type = TokenType::BREAK;
  }

  addToken(type);
}

char Lexer::advance()
{
  return source[current++];
}

bool Lexer::match(char expected)
{
  if (isAtEnd()) {
    return false;
  }
  if (source[current] != expected) {
    return false;
  }
  current++;
  return true;
}

char Lexer::peek()
{
  if (isAtEnd()) {
    return '\0';
  }
  return source[current];
}

char Lexer::peekNext()
{
  if (current + 1 >= source.length()) {
    return '\0';
  }
  return source[current + 1];
}

bool Lexer::isAtEnd()
{
  return current >= source.length();
}

bool Lexer::isDigit(char c)
{
  return c >= '0' && c <= '9';
}

bool Lexer::isAlpha(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Lexer::isAlphaNumeric(char c)
{
  return isAlpha(c) || isDigit(c);
}

void Lexer::addToken(TokenType type)
{
  std::string text = source.substr(start, current - start);
  tokens.emplace_back(type, text, filename, line, column);
}

void Lexer::templateString()
{
  while (!isAtEnd()) {
    if (peek() == '`') {
      if (current > start) {
        addToken(TokenType::TEMPLATE_STRING_PART);
      }
      start = current;
      advance();
      addToken(TokenType::BACKTICK);
      stateStack.pop_back();
      return;
    }

    if (peek() == '$' && peekNext() == '{') {
      if (current > start) {
        addToken(TokenType::TEMPLATE_STRING_PART);
      }
      start = current;
      advance(); // consume $
      advance(); // consume {
      addToken(TokenType::DOLLAR_BRACE);
      stateStack.push_back(LexerState::NORMAL);
      braceStack.push_back(0);
      return;
    }

    if (peek() == '\\') {
      advance();
      if (!isAtEnd()) {
        advance();
      }
    }
    else {
      if (peek() == '\n') {
        line++;
        lastNewline = current;
      }
      advance();
    }
  }

  reportError("Unterminated template string.");
}
