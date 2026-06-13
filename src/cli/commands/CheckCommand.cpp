#include "CheckCommand.h"

#include <fstream>
#include <iostream>

#include "../../lexer/Lexer.h"
#include "../../parser/Parser.h"
#include "../../utils/Logger.h"
#include "../ui/Printer.h"

namespace cli
{
void CheckCommand::execute(const std::string& filepath)
{
  std::ifstream file(filepath);
  if (!file.is_open()) {
    std::cerr << Printer::error("Could not open file: ") << filepath << std::endl;
    return;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string source = buffer.str();

  try {
    Lexer lexer(source, filepath);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto statements = parser.parse();
    std::cout << Printer::success("Syntax check passed for: ") << Printer::bold(filepath)
              << std::endl;
  }
  catch (const LoggerException& e) {
    std::cerr << Printer::error(e.errorType) << ": " << e.what() << std::endl;
    if (e.location.isValid()) {
      std::cerr << "  File \"" << e.location.file << "\", line " << e.location.line << std::endl;

      std::ifstream sourceFile(e.location.file);
      std::string lineContent;
      for (int i = 0; i < e.location.line; ++i) {
        if (!std::getline(sourceFile, lineContent)) {
          break;
        }
      }

      std::cerr << "    " << lineContent << std::endl;
      std::cerr << "    " << std::string(e.location.column > 0 ? e.location.column - 1 : 0, ' ')
                << "^" << std::endl;
    }
  }
  catch (const std::exception& e) {
    std::cerr << Printer::error("Error: ") << e.what() << std::endl;
  }
}
} // namespace cli
