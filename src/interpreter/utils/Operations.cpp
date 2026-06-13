#include <cmath>
#include <future>
#include <thread>
#include <type_traits>

#include "../../utils/Logger.h"
#include "../Interpreter.h"

gecko::Value Interpreter::callFunction(const gecko::Value& callee,
                                       const std::vector<gecko::Value>& arguments)
{
  return std::visit(
      gecko::overloaded{
          [&](const gecko::NativeFunction& func) -> gecko::Value {
            return func(arguments);
          },
          [&](const std::shared_ptr<UserFunction>& func) -> gecko::Value {
            if (func->isAsync) {
              auto promise = std::make_shared<PromiseValue>();
              auto capturedFunc = func;
              auto capturedArgs = arguments;
              auto capturedGlobals = globals;
              std::thread([capturedFunc, capturedArgs, capturedGlobals, promise]() {
                try {
                  auto interp = std::make_shared<Interpreter>("<async>", &std::cout);
                  Interpreter::current = interp.get();
                  interp->globals = capturedGlobals;
                  interp->environment = capturedFunc->closure;
                  gecko::Value asyncResult;
                  auto funcEnv = std::make_shared<Environment>(capturedFunc->closure);
                  for (size_t i = 0; i < capturedFunc->parameters.size(); ++i) {
                    funcEnv->define(capturedFunc->parameters[i].lexeme, capturedArgs[i]);
                  }
                  bool wasInFunction = interp->inFunction;
                  interp->inFunction = true;
                  try {
                    interp->executeBlock(capturedFunc->body, funcEnv);
                    asyncResult = nullptr;
                  }
                  catch (const ReturnException& e) {
                    asyncResult = e.value;
                  }
                  interp->inFunction = wasInFunction;
                  promise->resolve(std::move(asyncResult));
                }
                catch (...) {
                  promise->p->set_exception(std::current_exception());
                }
              }).detach();
              return promise;
            }
            return callUserFunction(func, arguments, func->closure);
          },
          [&](const std::shared_ptr<UserClass>& klass) -> gecko::Value {
            auto instance = std::make_shared<ClassInstance>(klass);
            auto constructorIt = klass->methods.find("constructor");
            if (constructorIt != klass->methods.end()) {
              auto& constructorDef = constructorIt->second;
              auto constructor = constructorDef.regular;
              if (arguments.size() != constructor->parameters.size()) {
                throw std::runtime_error(
                    "Expected " + std::to_string(constructor->parameters.size())
                    + " arguments but got " + std::to_string(arguments.size()));
              }
              gecko::Value initializerResult =
                  callUserFunction(constructor, arguments, constructor->closure, instance);
              if (initializerResult.isInstance()) {
                return initializerResult;
              }
            }
            return instance;
          },
          [](const auto&) -> gecko::Value {
            throw std::runtime_error("Can only call functions and classes");
          }},
      callee.data);
}

gecko::Value Interpreter::callUserFunction(const std::shared_ptr<UserFunction>& func,
                                           const std::vector<gecko::Value>& arguments,
                                           std::shared_ptr<Environment> env,
                                           const gecko::Value& thisInstance)
{
  if (++recursionDepth > MAX_RECURSION_DEPTH) {
    throw std::runtime_error("Stack overflow: Maximum recursion depth exceeded.");
  }
  if (arguments.size() != func->parameters.size()) {
    throw std::runtime_error("Expected " + std::to_string(func->parameters.size())
                             + " arguments but got " + std::to_string(arguments.size()));
  }
  SourceLocation loc(func->parameters.empty() ? func->declaration.getLocation()
                                              : func->parameters[0].getLocation());
  Logger::pushFrame(func->name, loc);
  auto funcEnv = std::make_shared<Environment>(env);
  for (size_t i = 0; i < func->parameters.size(); ++i) {
    funcEnv->define(func->parameters[i].lexeme, arguments[i]);
  }
  bool wasInFunction = inFunction;
  inFunction = true;
  bool pushedThis = false;
  if (!thisInstance.isNil()) {
    thisStack.push_back(thisInstance);
    pushedThis = true;
  }
  gecko::Value resultValue;
  try {
    executeBlock(func->body, funcEnv);
    resultValue = nullptr;
  }
  catch (const ReturnException& e) {
    resultValue = e.value;
  }
  catch (const std::exception& e) {
    if (pushedThis) {
      thisStack.pop_back();
    }
    inFunction = wasInFunction;
    auto trace = Logger::captureTrace();
    Logger::popFrame();
    recursionDepth--;
    if (dynamic_cast<const RuntimeError*>(&e)) {
      throw;
    }
    throw RuntimeError(e.what(), std::move(trace));
  }
  catch (...) {
    if (pushedThis) {
      thisStack.pop_back();
    }
    inFunction = wasInFunction;
    Logger::popFrame();
    recursionDepth--;
    throw;
  }
  if (pushedThis) {
    thisStack.pop_back();
  }
  inFunction = wasInFunction;
  Logger::popFrame();
  recursionDepth--;
  return resultValue;
}

gecko::Value
Interpreter::binaryOp(const gecko::Value& left, const Token& op, const gecko::Value& right)
{
  return std::visit(
      gecko::overloaded{
          [&](double l, double r) -> gecko::Value {
            switch (op.type) {
              case TokenType::PLUS:
                return l + r;
              case TokenType::MINUS:
                return l - r;
              case TokenType::MULTIPLY:
                return l * r;
              case TokenType::DIVIDE:
                if (r == 0.0) {
                  throw std::runtime_error("Division by zero at line " + std::to_string(op.line));
                }
                return l / r;
              case TokenType::MODULO:
                if (r == 0.0) {
                  throw std::runtime_error("Modulo by zero at line " + std::to_string(op.line));
                }
                return std::fmod(l, r);
              case TokenType::EQUAL:
                return l == r;
              case TokenType::NOT_EQUAL:
                return l != r;
              case TokenType::LESS:
                return l < r;
              case TokenType::LESS_EQUAL:
                return l <= r;
              case TokenType::GREATER:
                return l > r;
              case TokenType::GREATER_EQUAL:
                return l >= r;
              default:
                break;
            }
            throw std::runtime_error("Invalid numeric operation");
          },
          [&](const std::string& l, const std::string& r) -> gecko::Value {
            if (op.type == TokenType::PLUS) {
              return l + r;
            }
            if (op.type == TokenType::EQUAL) {
              return l == r;
            }
            if (op.type == TokenType::NOT_EQUAL) {
              return l != r;
            }
            throw std::runtime_error("Invalid string operation");
          },
          [&](bool l, bool r) -> gecko::Value {
            switch (op.type) {
              case TokenType::EQUAL:
                return l == r;
              case TokenType::NOT_EQUAL:
                return l != r;
              case TokenType::PLUS:
                return (l ? 1.0 : 0.0) + (r ? 1.0 : 0.0);
              default:
                break;
            }
            throw std::runtime_error("Invalid boolean operation");
          },
          [&](double l, bool r) -> gecko::Value {
            switch (op.type) {
              case TokenType::PLUS:
                return l + (r ? 1.0 : 0.0);
              case TokenType::MINUS:
                return l - (r ? 1.0 : 0.0);
              case TokenType::MULTIPLY:
                return l * (r ? 1.0 : 0.0);
              case TokenType::DIVIDE:
                if (!r) {
                  throw std::runtime_error("Division by zero at line " + std::to_string(op.line));
                }
                return l;
              default:
                break;
            }
            throw std::runtime_error("Invalid number-boolean operation");
          },
          [&](bool l, double r) -> gecko::Value {
            switch (op.type) {
              case TokenType::PLUS:
                return (l ? 1.0 : 0.0) + r;
              case TokenType::MINUS:
                return (l ? 1.0 : 0.0) - r;
              case TokenType::MULTIPLY:
                return (l ? 1.0 : 0.0) * r;
              case TokenType::DIVIDE:
                if (r == 0.0) {
                  throw std::runtime_error("Division by zero at line " + std::to_string(op.line));
                }
                return (l ? 1.0 : 0.0) / r;
              default:
                break;
            }
            throw std::runtime_error("Invalid boolean-number operation");
          },
          [&](const auto& l, const auto& r) -> gecko::Value {
            if (op.type == TokenType::PLUS) {
              if constexpr (std::is_same_v<std::decay_t<decltype(l)>, std::string>) {
                return l + stringify(gecko::Value{r});
              }
              if constexpr (std::is_same_v<std::decay_t<decltype(r)>, std::string>) {
                return stringify(gecko::Value{l}) + r;
              }
              if constexpr (std::is_same_v<std::decay_t<decltype(l)>, gecko::Nil>) {
                return "null" + stringify(gecko::Value{r});
              }
              if constexpr (std::is_same_v<std::decay_t<decltype(r)>, gecko::Nil>) {
                return stringify(gecko::Value{l}) + "null";
              }
            }
            if (op.type == TokenType::EQUAL) {
              return false;
            }
            if (op.type == TokenType::NOT_EQUAL) {
              return true;
            }
            throw std::runtime_error("Incompatible types for operator");
          }},
      left.data,
      right.data);
}

gecko::Value Interpreter::unaryOp(const Token& op, const gecko::Value& right)
{
  return std::visit(gecko::overloaded{[&](double d) -> gecko::Value {
                                        if (op.type == TokenType::MINUS) {
                                          return -d;
                                        }
                                        throw std::runtime_error("Invalid numeric unary operator");
                                      },
                                      [&](const auto& r) -> gecko::Value {
                                        if (op.type == TokenType::NOT) {
                                          return !isTruthy(r);
                                        }
                                        if (op.type == TokenType::MINUS) {
                                          throw std::runtime_error(
                                              "Operand must be a number for unary minus at line "
                                              + std::to_string(op.line));
                                        }
                                        throw std::runtime_error("Unknown unary operator");
                                      }},
                    right.data);
}
