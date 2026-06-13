#include <iostream>
#include <map>

#include "../Interpreter.h"
#include "../module/ModuleLoader.h"

namespace
{
class BreakException
{};
} // namespace

void Interpreter::visitExpressionStmt(ExpressionStmt* stmt)
{
  evaluate(stmt->expression.get());
}

void Interpreter::visitPrintStmt(PrintStmt* stmt)
{
  auto value = evaluate(stmt->expression.get());
  std::cout << stringify(value) << std::endl;
}

void Interpreter::visitVarStmt(VarStmt* stmt)
{
  gecko::Value value = nullptr;
  if (stmt->initializer) {
    value = evaluate(stmt->initializer.get());
  } else if (!stmt->isConst) {
    value = 0.0;
  }
  environment->define(stmt->name.lexeme, value, stmt->isConst);
}

void Interpreter::visitBlockStmt(BlockStmt* stmt)
{
  auto newEnv = std::make_shared<Environment>(environment);
  executeBlock(stmt->statements, newEnv);
}

void Interpreter::visitIfStmt(IfStmt* stmt)
{
  if (isTruthy(evaluate(stmt->condition.get()))) {
    execute(stmt->thenBranch.get());
  }
  else if (stmt->elseBranch) {
    execute(stmt->elseBranch.get());
  }
}

void Interpreter::visitWhileStmt(WhileStmt* stmt)
{
  ++loopDepth;
  try {
    while (isTruthy(evaluate(stmt->condition.get()))) {
      execute(stmt->body.get());
    }
  }
  catch (const BreakException&) {
    --loopDepth;
    return;
  }
  catch (...) {
    --loopDepth;
    throw;
  }
  --loopDepth;
}

void Interpreter::visitForStmt(ForStmt* stmt)
{
  if (stmt->initializer) {
    execute(stmt->initializer.get());
  }
  ++loopDepth;
  try {
    while (!stmt->condition || isTruthy(evaluate(stmt->condition.get()))) {
      execute(stmt->body.get());
      if (stmt->increment) {
        evaluate(stmt->increment.get());
      }
    }
  }
  catch (const BreakException&) {
    --loopDepth;
    return;
  }
  catch (...) {
    --loopDepth;
    throw;
  }
  --loopDepth;
}

void Interpreter::visitReturnStmt(ReturnStmt* stmt)
{
  gecko::Value value = nullptr;
  if (stmt->value) {
    value = evaluate(stmt->value.get());
  }
  if (inFunction) {
    throw ReturnException(value);
  }
  throw std::runtime_error("Return statement used outside of function");
}

void Interpreter::visitBreakStmt(BreakStmt* stmt)
{
  if (loopDepth <= 0) {
    throw std::runtime_error("Break statement used outside of loop at line "
                             + std::to_string(stmt->keyword.line));
  }
  throw BreakException();
}

void Interpreter::visitFunctionStmt(FunctionStmt* stmt)
{
  auto func = std::make_shared<UserFunction>(stmt->name.lexeme,
                                             stmt->parameters,
                                             stmt->body,
                                             stmt->name,
                                             environment,
                                             false, false, false,
                                             stmt->isAsync);
  environment->define(stmt->name.lexeme, func);
}

void Interpreter::visitClassStmt(ClassStmt* stmt)
{
  std::shared_ptr<UserClass> superclass = nullptr;
  if (stmt->superclass) {
    auto superValue = evaluate(stmt->superclass.get());
    if (!superValue.isUserClass()) {
      throw std::runtime_error("Superclass must be a class.");
    }
    superclass = superValue.as<std::shared_ptr<UserClass>>();
  }
  std::map<std::string, MethodDefinition> methods;
  std::map<std::string, MethodDefinition> staticMethods;
  for (auto& methodStmt : stmt->methods) {
    auto func = std::make_shared<UserFunction>(methodStmt->name.lexeme,
                                               methodStmt->parameters,
                                               methodStmt->body,
                                               methodStmt->name,
                                               environment,
                                               methodStmt->isStatic,
                                               methodStmt->isGetter,
                                               methodStmt->isSetter);

    auto& targetMap = (methodStmt->isStatic) ? staticMethods : methods;
    MethodDefinition& method = targetMap[methodStmt->name.lexeme];

    if (methodStmt->isGetter) {
      method.getter = func;
    }
    else if (methodStmt->isSetter) {
      method.setter = func;
    }
    else {
      method.regular = func;
    }
  }
  auto klass = std::make_shared<UserClass>(stmt->name.lexeme,
                                           superclass,
                                           std::move(methods),
                                           std::move(staticMethods));

  environment->define(stmt->name.lexeme, klass);
}

void Interpreter::visitTryStmt(TryStmt* stmt)
{
  try {
    execute(stmt->tryBlock.get());
  }
  catch (const ReturnException&) {
    throw;
  }
  catch (const BreakException&) {
    throw;
  }
  catch (...) {
    auto env = std::make_shared<Environment>(environment);
    try {
      throw;
    }
    catch (const std::exception& e) {
      env->define(stmt->exceptionVar.lexeme, std::string(e.what()));
    }
    catch (...) {
      env->define(stmt->exceptionVar.lexeme, std::string("Unknown error"));
    }
    executeBlock(stmt->catchBlock->statements, env);
  }
}

void Interpreter::visitPackageStmt(PackageStmt* stmt)
{
  currentPackage = stmt->name.lexeme;
  environment->setPackageName(currentPackage);
}

void Interpreter::visitExportStmt(ExportStmt* stmt)
{
  for (const auto& name : stmt->names) {
    try {
      environment->get(name.lexeme);
      environment->exportName(name.lexeme);
    }
    catch (...) {
      throw LoggerException("ExportError: cannot export name '" + name.lexeme + "'",
                            name.getLocation(),
                            "RuntimeError");
    }
  }
}

void Interpreter::visitImportStmt(ImportStmt* stmt)
{
  try {
    ModuleLoader loader(globals, moduleCache, sourceFile);
    auto moduleEnv = loader.loadModule(stmt->modulePath);
    if (stmt->importAll) {
      std::string pkgName = moduleEnv->getPackageName();
      if (!pkgName.empty()) {
        environment->define(pkgName, moduleEnv);
      }
      auto names = moduleEnv->getExportedNames();
      for (const auto& name : names) {
        environment->define(name, moduleEnv->get(name));
      }
    }
    else if (!stmt->names.empty()) {
      if (stmt->isNamespaceImport) {
        environment->define(stmt->names[0].lexeme, moduleEnv);
      }
      else {
        for (const auto& name : stmt->names) {
          try {
            if (!moduleEnv->isExported(name.lexeme)) {
              throw std::runtime_error("ExportError: name '" + name.lexeme
                                       + "' is not exported by module '" + stmt->modulePath + "'");
            }
            auto value = moduleEnv->get(name.lexeme);
            std::string targetName = stmt->aliases.at(name.lexeme);
            environment->define(targetName, value);
          }
          catch (const std::runtime_error& e) {
            if (std::string(e.what()).find("ExportError") != std::string::npos) {
              throw;
            }
            throw std::runtime_error("ImportError: cannot import name '" + name.lexeme + "' from '"
                                     + stmt->modulePath + "'");
          }
          catch (...) {
            throw std::runtime_error("ImportError: cannot import name '" + name.lexeme + "' from '"
                                     + stmt->modulePath + "'");
          }
        }
      }
    }
  }
  catch (const std::exception& e) {
    SourceLocation loc =
        stmt->names.empty() ? SourceLocation(sourceFile, 0, 0) : stmt->names[0].getLocation();
    throw LoggerException(e.what(), loc, "RuntimeError");
  }
}
