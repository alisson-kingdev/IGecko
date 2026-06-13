#include <string>
#include <vector>

#include "../Interpreter.h"

void Interpreter::visitBinaryExpr(BinaryExpr* expr)
{
  auto left = evaluate(expr->left.get());
  auto right = evaluate(expr->right.get());
  result = binaryOp(left, expr->op, right);
}

void Interpreter::visitGroupingExpr(GroupingExpr* expr)
{
  result = evaluate(expr->expression.get());
}

void Interpreter::visitLiteralExpr(LiteralExpr* expr)
{
  switch (expr->value.type) {
    case TokenType::STRING:
      result = decodeStringLiteral(expr->value.lexeme);
      break;
    case TokenType::TEMPLATE_STRING_PART:
      result = decodeStringLiteral(expr->value.lexeme, false);
      break;
    case TokenType::NUMBER:
      result = std::stod(expr->value.lexeme);
      break;
    case TokenType::TRUE:
      result = true;
      break;
    case TokenType::FALSE:
      result = false;
      break;
    case TokenType::NULL_VAL:
      result = nullptr;
      break;
    default:
      result = expr->value.lexeme;
      break;
  }
}

void Interpreter::visitUnaryExpr(UnaryExpr* expr)
{
  auto right = evaluate(expr->right.get());
  result = unaryOp(expr->op, right);
}

void Interpreter::visitVariableExpr(VariableExpr* expr)
{
  result = environment->get(expr->name.lexeme);
}

void Interpreter::visitAssignExpr(AssignExpr* expr)
{
  auto value = evaluate(expr->value.get());
  environment->assign(expr->name.lexeme, value);
  result = value;
}

void Interpreter::visitLogicalExpr(LogicalExpr* expr)
{
  auto left = evaluate(expr->left.get());
  if (expr->op.type == TokenType::OR) {
    if (isTruthy(left)) {
      result = left;
      return;
    }
  }
  else if (expr->op.type == TokenType::AND) {
    if (!isTruthy(left)) {
      result = left;
      return;
    }
  }
  result = evaluate(expr->right.get());
}

void Interpreter::visitCallExpr(CallExpr* expr)
{
  auto callee = evaluate(expr->callee.get());
  std::vector<gecko::Value> arguments;
  for (const auto& arg : expr->arguments) {
    arguments.push_back(evaluate(arg.get()));
  }
  result = callFunction(callee, arguments);
}

void Interpreter::visitAwaitExpr(AwaitExpr* expr)
{
  auto value = evaluate(expr->expr.get());
  if (value.isPromise()) {
    result = value.as<gecko::Promise>()->get();
  }
  else {
    result = value;
  }
}

void Interpreter::visitIncrementExpr(IncrementExpr* expr)
{
  auto value = environment->get(expr->name.lexeme);
  double num = value.isNumber() ? value.as<double>() : 0.0;
  double newVal = num + ((expr->op.type == TokenType::PLUS_PLUS) ? 1.0 : -1.0);
  environment->assign(expr->name.lexeme, newVal);
  result = value;
}

void Interpreter::visitGetExpr(GetExpr* expr)
{
  auto object = evaluate(expr->object.get());
  const auto& name = expr->name.lexeme;

  if (name == "type") {
    result =
        gecko::NativeFunction([this, object](const std::vector<gecko::Value>&) -> gecko::Value {
          return getType(object);
        });
    return;
  }

  if (object.isString()) {
    auto value = object.as<std::string>();
    if (name == "length") {
      result = static_cast<double>(value.size());
      return;
    }
    if (name == "charAt") {
      result =
          gecko::NativeFunction([value](const std::vector<gecko::Value>& args) -> gecko::Value {
            if (args.empty() || !args[0].isNumber()) {
              return std::string("");
            }
            auto index = static_cast<int>(args[0].as<double>());
            if (index < 0 || static_cast<size_t>(index) >= value.size()) {
              return std::string("");
            }
            return std::string(1, value[static_cast<size_t>(index)]);
          });
      return;
    }
    if (name == "charCodeAt") {
      result = gecko::NativeFunction([value](
                                         const std::vector<gecko::Value>& args) -> gecko::Value {
        if (args.empty() || !args[0].isNumber()) {
          return -1.0;
        }
        auto index = static_cast<int>(args[0].as<double>());
        if (index < 0 || static_cast<size_t>(index) >= value.size()) {
          return -1.0;
        }
        return static_cast<double>(static_cast<unsigned char>(value[static_cast<size_t>(index)]));
      });
      return;
    }
    if (name == "substring" || name == "slice") {
      result =
          gecko::NativeFunction([value](const std::vector<gecko::Value>& args) -> gecko::Value {
            if (args.empty() || !args[0].isNumber()) {
              return std::string("");
            }
            auto start = static_cast<int>(args[0].as<double>());
            auto end = static_cast<int>(value.size());
            if (args.size() > 1 && args[1].isNumber()) {
              end = static_cast<int>(args[1].as<double>());
            }
            if (start < 0) {
              start = 0;
            }
            if (end < start) {
              end = start;
            }
            if (static_cast<size_t>(start) > value.size()) {
              return std::string("");
            }
            if (static_cast<size_t>(end) > value.size()) {
              end = static_cast<int>(value.size());
            }
            return value.substr(static_cast<size_t>(start), static_cast<size_t>(end - start));
          });
      return;
    }
    if (name == "indexOf") {
      result =
          gecko::NativeFunction([value](const std::vector<gecko::Value>& args) -> gecko::Value {
            if (args.empty() || !args[0].isString()) {
              return -1.0;
            }
            auto pos = value.find(args[0].as<std::string>());
            return pos == std::string::npos ? -1.0 : static_cast<double>(pos);
          });
      return;
    }
    if (name == "toUpperCase" || name == "toLowerCase") {
      bool upper = name == "toUpperCase";
      result =
          gecko::NativeFunction([value, upper](const std::vector<gecko::Value>&) -> gecko::Value {
            std::string out = value;
            for (auto& ch : out) {
              auto c = static_cast<unsigned char>(ch);
              ch = static_cast<char>(upper ? std::toupper(c) : std::tolower(c));
            }
            return out;
          });
      return;
    }
    if (name == "trim") {
      result = gecko::NativeFunction([value](const std::vector<gecko::Value>&) -> gecko::Value {
        size_t start = 0;
        while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
          ++start;
        }
        size_t end = value.size();
        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
          --end;
        }
        return value.substr(start, end - start);
      });
      return;
    }
    if (name == "replace") {
      result =
          gecko::NativeFunction([value](const std::vector<gecko::Value>& args) -> gecko::Value {
            if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
              return value;
            }
            auto searchValue = args[0].as<std::string>();
            auto replaceValue = args[1].as<std::string>();
            auto pos = value.find(searchValue);
            if (pos == std::string::npos) {
              return value;
            }
            auto out = value;
            out.replace(pos, searchValue.size(), replaceValue);
            return out;
          });
      return;
    }
    if (name == "repeat") {
      result =
          gecko::NativeFunction([value](const std::vector<gecko::Value>& args) -> gecko::Value {
            if (args.empty() || !args[0].isNumber()) {
              return std::string("");
            }
            auto count = static_cast<int>(args[0].as<double>());
            std::string out;
            for (int i = 0; i < count; ++i) {
              out += value;
            }
            return out;
          });
      return;
    }
  }

  if (object.isArray()) {
    auto vec = object.as<gecko::Array>();
    const auto& propName = expr->name.lexeme;
    if (propName == "length" || propName == "size") {
      result = static_cast<double>(vec->size());
      return;
    }
    if (name == "push") {
      result = gecko::NativeFunction([vec](const std::vector<gecko::Value>& args) -> gecko::Value {
        for (const auto& arg : args) {
          vec->push_back(arg);
        }
        return nullptr;
      });
      return;
    }
  }

  if (object.isInstance()) {
    auto instance = object.as<std::shared_ptr<ClassInstance>>();

    // Check for getter first
    auto methodIt = instance->klass->methods.find(expr->name.lexeme);
    if (methodIt != instance->klass->methods.end()) {
      auto& methodDef = methodIt->second;
      if (methodDef.getter) {
        result = callUserFunction(methodDef.getter, {}, methodDef.getter->closure, instance);
        return;
      }
    }

    // Fallback to field lookup
    auto fieldIt = instance->fields.find(expr->name.lexeme);
    if (fieldIt != instance->fields.end()) {
      result = fieldIt->second;
      return;
    }

    // Check for method (regular)
    if (methodIt != instance->klass->methods.end() && methodIt->second.regular) {
      auto method = methodIt->second.regular;
      result = gecko::NativeFunction(
          [this, instance, method](const std::vector<gecko::Value>& args) -> gecko::Value {
            return callUserFunction(method, args, method->closure, instance);
          });
      return;
    }
  }

  if (object.isUserClass()) {
    auto klass = object.as<std::shared_ptr<UserClass>>();
    auto staticIt = klass->staticMethods.find(expr->name.lexeme);
    if (staticIt != klass->staticMethods.end()) {
      auto method = staticIt->second.regular;
      result = gecko::NativeFunction(
          [this, method](const std::vector<gecko::Value>& args) -> gecko::Value {
            return callUserFunction(method, args, method->closure, nullptr);
          });
      return;
    }
  }

  if (object.isObject()) {
    auto moduleEnv = object.as<gecko::Object>();
    try {
      if (!moduleEnv->isExported(expr->name.lexeme)) {
        throw std::runtime_error("ExportError: name '" + expr->name.lexeme + "' is not exported.");
      }
      result = moduleEnv->get(expr->name.lexeme);
      return;
    }
    catch (const std::runtime_error& e) {
      if (std::string(e.what()).find("ExportError") != std::string::npos) {
        throw;
      }
    }
  }
  throw std::runtime_error("Undefined property '" + expr->name.lexeme + "'.");
}

void Interpreter::visitSetExpr(SetExpr* expr)
{
  auto object = evaluate(expr->object.get());
  auto value = evaluate(expr->value.get());
  if (object.isInstance()) {
    auto instance = object.as<std::shared_ptr<ClassInstance>>();

    // Check for setter first
    auto methodIt = instance->klass->methods.find(expr->name.lexeme);
    if (methodIt != instance->klass->methods.end()) {
      auto& methodDef = methodIt->second;
      if (methodDef.setter) {
        callUserFunction(methodDef.setter, {value}, methodDef.setter->closure, instance);
        result = value;
        return;
      }
    }

    instance->fields[expr->name.lexeme] = value;
    result = value;
    return;
  }
  if (object.isObject()) {
    auto env = object.as<gecko::Object>();
    auto val = evaluate(expr->value.get());
    env->define(expr->name.lexeme, val);
    env->exportName(expr->name.lexeme);
    result = val;
    return;
  }
  throw std::runtime_error("Property assignment is only supported on class instances.");
}

void Interpreter::visitThisExpr(ThisExpr* expr)
{
  if (thisStack.empty()) {
    throw std::runtime_error("'this' keyword used outside of method at line "
                             + std::to_string(expr->keyword.line));
  }
  result = thisStack.back();
}

void Interpreter::visitTemplateExpr(TemplateExpr* expr)
{
  std::string finalString = "";
  for (const auto& part : expr->parts) {
    gecko::Value val = evaluate(part.get());
    finalString += stringify(val);
  }
  result = finalString;
}

void Interpreter::visitArrayExpr(ArrayExpr* expr)
{
  auto elements = std::make_shared<std::vector<gecko::Value>>();
  for (const auto& element : expr->elements) {
    elements->push_back(evaluate(element.get()));
  }
  result = elements;
}

void Interpreter::visitObjectExpr(ObjectExpr* expr)
{
  auto objEnv = std::make_shared<Environment>();
  for (const auto& [key, valueExpr] : expr->properties) {
    objEnv->define(key, evaluate(valueExpr.get()));
    objEnv->exportName(key);
  }
  result = objEnv;
}

void Interpreter::visitFunctionExpr(FunctionExpr* expr)
{
  Token dummyToken(TokenType::FUNC, "func", 0, 0);

  auto function = std::make_shared<UserFunction>(
      "<anonymous>",
      expr->parameters,
      expr->body,
      dummyToken,
      environment,
      false, false, false,
      expr->isAsync);

  result = function;
}

void Interpreter::visitTernaryExpr(TernaryExpr* expr)
{
  if (isTruthy(evaluate(expr->condition.get()))) {
    result = evaluate(expr->thenBranch.get());
  }
  else {
    result = evaluate(expr->elseBranch.get());
  }
}
