#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "cli/repl/Repl.h"
#include "interpreter/Interpreter.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "utils/Logger.h"
#include "utils/Version.h"
#include "vm/Vm.h"

std::string readFile(const std::string& path)
{
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("Could not open file: " + path);
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

void runScript(const std::string& filepath, const std::string& backend)
{
  Logger::configure(LogLevel::INFO, true);
  Logger::setCurrentFile(filepath);

  std::string source = readFile(filepath);

  if (backend == "vm") {
    Vm vm;
    auto result = vm.interpret(source, filepath);
    if (result.status == InterpretResult::COMPILE_ERROR) {
      std::cerr << "VM Compile error: " << result.message << std::endl;
    }
    if (result.status == InterpretResult::RUNTIME_ERROR) {
      std::cerr << "VM Runtime error: " << result.message << std::endl;
    }
    return;
  }

  // Default: tree-walking interpreter
  Lexer lexer(source, filepath);
  auto tokens = lexer.tokenize();

  Parser parser(tokens);
  auto statements = parser.parse();

  Interpreter interpreter(filepath);
  interpreter.interpret(statements);
}

void watchFile(const std::string& filepath, const std::string& backend)
{
  auto lastWrite = std::filesystem::last_write_time(filepath);
  std::cout << "[watch] Watching " << filepath << " for changes..." << std::endl;

  runScript(filepath, backend);

  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    try {
      auto currentWrite = std::filesystem::last_write_time(filepath);
      if (currentWrite != lastWrite) {
        lastWrite = currentWrite;
        std::cout << "\n[watch] File changed, reloading..." << std::endl;
        runScript(filepath, backend);
      }
    }
    catch (const std::exception& e) {
      std::cerr << "[watch] Error: " << e.what() << std::endl;
    }
  }
}

int main(int argc, char* argv[])
{
  std::string backend = "tree";
  std::string filepath;
  bool watchMode = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--backend" && i + 1 < argc) {
      backend = argv[++i];
    } else if (arg == "--watch" || arg == "-w") {
      watchMode = true;
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: gecko [--backend=tree|vm] [--watch] <script.gk>" << std::endl;
      std::cout << "Version: " << gecko::getVersion() << std::endl;
      std::cout << std::endl;
      std::cout << "Options:" << std::endl;
      std::cout << "  --backend=tree|vm   Select interpreter backend" << std::endl;
      std::cout << "  --watch, -w         Watch file for changes and reload" << std::endl;
      std::cout << "  --help, -h          Show this help" << std::endl;
      return 0;
    } else if (arg.rfind("--backend=", 0) == 0) {
      backend = arg.substr(10);
    } else if (arg[0] != '-') {
      filepath = arg;
    }
  }

  if (filepath.empty()) {
    Repl repl;
    repl.run();
    return 0;
  }

  try {
    if (watchMode) {
      watchFile(filepath, backend);
    } else {
      runScript(filepath, backend);
    }
  }
  catch (const LoggerException& e) {
    Logger::error(e.what(), e.location, e.errorType);
    return 1;
  }
  catch (const RuntimeError& e) {
    Logger::error(e.what(), e.trace, SourceLocation(filepath, 0, 0), "RuntimeError");
    return 1;
  }
  catch (const ReturnException&) {
    Logger::error("Return outside of function", SourceLocation(filepath, 0, 0), "RuntimeError");
    return 1;
  }
  catch (const std::exception& e) {
    Logger::error(e.what(), SourceLocation(filepath, 0, 0), "RuntimeError");
    return 1;
  }

  return 0;
}
