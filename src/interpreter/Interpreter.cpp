#include "Interpreter.h"

#include "../utils/Logger.h"

thread_local Interpreter* Interpreter::current = nullptr;

Interpreter::Interpreter(std::string filename, std::ostream* out)
    : outStream(out), sourceFile(std::move(filename))
{
  globals = std::make_shared<Environment>();
  environment = globals;
  moduleCache = std::make_shared<ModuleCache>();
  Logger::setCurrentFile(sourceFile);
  defineBuiltins();
}

void Interpreter::interpret(const std::vector<std::shared_ptr<Stmt>>& statements)
{
  current = this;
  for (const auto& stmt : statements) {
    execute(stmt.get());
  }
}

void Interpreter::executeStatements(const std::vector<std::shared_ptr<Stmt>>& statements)
{
  for (const auto& stmt : statements) {
    execute(stmt.get());
  }
}

void Interpreter::execute(Stmt* stmt)
{
  if (stmt) {
    stmt->accept(this);
  }
}

void Interpreter::executeBlock(const std::vector<std::shared_ptr<Stmt>>& statements,
                               std::shared_ptr<Environment> env)
{
  auto previous = environment;
  environment = env;
  try {
    for (const auto& stmt : statements) {
      execute(stmt.get());
    }
  }
  catch (...) {
    environment = previous;
    throw;
  }
  environment = previous;
}

gecko::Value Interpreter::evaluate(Expr* expr)
{
  if (!expr) {
    return gecko::Value(); // Return a default value or handle accordingly
  }
  expr->accept(this);
  return result;
}
