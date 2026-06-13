#include "ExecuteCommand.h"

#include <fstream>
#include <iostream>

#include "../../interpreter/Interpreter.h"
#include "../../lexer/Lexer.h"
#include "../../parser/Parser.h"
#include "../../utils/Logger.h"
#include "../ui/Printer.h"

namespace cli
{
void ExecuteCommand::execute(const std::string& filepath)
{
  std::ifstream file(filepath);
  if (!file.is_open()) {
    std::cerr << Printer::error("Could not open file: ") << filepath << std::endl;
    return;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string source = buffer.str();

  Logger::debug("Lexing source code...");
  Lexer lexer(source, filepath);
  auto tokens = lexer.tokenize();
  Logger::debug("Lexing completed. Found " + std::to_string(tokens.size()) + " tokens.");

  Logger::debug("Parsing tokens into AST...");
  Parser parser(tokens);
  auto statements = parser.parse();
  Logger::debug("Parsing completed. Generated " + std::to_string(statements.size())
                + " statements.");

  Logger::debug("Initializing interpreter...");
  Interpreter interpreter(filepath);

  Logger::debug("Starting interpretation...");
  interpreter.interpret(statements);
  Logger::debug("Interpretation finished.");
}
} // namespace cli
