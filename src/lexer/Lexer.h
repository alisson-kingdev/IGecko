#ifndef LEXER_H
#define LEXER_H

#include <memory>
#include <string>
#include <vector>

#include "../utils/Logger.h"

/**
 * @brief Token types for the IGecko language.
 */
enum class TokenType
{
  // Ternary
  QUESTION,

  // Literals
  IDENTIFIER,
  STRING,
  NUMBER,

  BOOLEAN,

  // Keywords
  PACKAGE,
  FUNC,
  VAR,
  CONST,
  IF,
  ELSE,
  FOR,
  WHILE,
  RETURN,
  CLASS,
  CONSTRUCTOR,
  STATIC,
  NEW,
  GET,
  SET,
  THIS,
  TRY,
  CATCH,
  FROM,
  AS,
  EXPORT,
  IMPORT,
  TRUE,
  FALSE,
  NULL_VAL,
  PRINT,
  BREAK,
  ASYNC,
  AWAIT,

  // Operators
  PLUS,
  PLUS_PLUS,
  MINUS,
  MINUS_MINUS,
  MULTIPLY,
  DIVIDE,
  MODULO,
  ASSIGN,
  EQUAL,
  NOT_EQUAL,
  LESS,
  GREATER,
  LESS_EQUAL,
  GREATER_EQUAL,
  AND,
  OR,
  NOT,

  // Delimiters
  LEFT_PAREN,
  RIGHT_PAREN,
  LEFT_BRACE,
  RIGHT_BRACE,
  LEFT_BRACKET,
  RIGHT_BRACKET,
  COLON,
  SEMICOLON,
  COMMA,
  DOT,
  ARROW,

  // Template Strings
  BACKTICK,
  DOLLAR_BRACE,
  TEMPLATE_STRING_PART,

  // Special
  EOF_TOKEN
};

/**
 * @brief Represents a lexical token in the source code.
 */
struct Token
{
  TokenType type;
  std::string lexeme;
  std::string file;
  int line;
  int column;

  Token(TokenType t, const std::string& l, std::string f, int ln, int col)
      : type(t), lexeme(l), file(std::move(f)), line(ln), column(col)
  {}

  Token(TokenType t, const std::string& l, int ln, int col)
      : type(t), lexeme(l), file(Logger::getCurrentFile()), line(ln), column(col)
  {}

  /** @brief Returns the source location of this token. */
  SourceLocation getLocation() const
  {
    return SourceLocation(file, line, column);
  }
};

/**
 * @brief Lexer state for handling nested template strings.
 */
enum class LexerState
{
  NORMAL,
  TEMPLATE
};

/**
 * @brief Converts raw source code into a stream of tokens.
 */
class Lexer
{
public:
  /** @brief Constructs a lexer for the given source input. */
  explicit Lexer(const std::string& source, const std::string& filename);

  /** @brief Tokenizes the input source. */
  std::vector<Token> tokenize();

private:
  std::string source;
  std::string filename;
  std::vector<Token> tokens;
  size_t current = 0;
  size_t start = 0;
  int line = 1;
  int column = 1;
  size_t lastNewline = 0;

  std::vector<LexerState> stateStack;
  std::vector<int> braceStack;

  void scanToken();
  void templateString();
  void string();
  void singleQuoteString();
  void number();
  void identifier();
  void reportError(const std::string& message);

  char advance();
  bool match(char expected);
  char peek();
  char peekNext();

  bool isAtEnd();
  bool isDigit(char c);
  bool isAlpha(char c);
  bool isAlphaNumeric(char c);

  void addToken(TokenType type);
};

#endif // LEXER_H
