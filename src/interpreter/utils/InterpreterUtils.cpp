#include <algorithm>
#include <cmath>
#include <sstream>

#include "../Interpreter.h"

std::string Interpreter::decodeStringLiteral(const std::string& lexeme, bool quoted)
{
  std::string raw =
      quoted ? (lexeme.size() >= 2 ? lexeme.substr(1, lexeme.size() - 2) : lexeme) : lexeme;
  std::string out;
  out.reserve(raw.size());

  auto hexValue = [](char c) -> int {
    if (c >= '0' && c <= '9') {
      return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
      return 10 + c - 'a';
    }
    if (c >= 'A' && c <= 'F') {
      return 10 + c - 'A';
    }
    return -1;
  };

  for (size_t i = 0; i < raw.size(); ++i) {
    char c = raw[i];
    if (c != '\\' || i + 1 >= raw.size()) {
      out += c;
      continue;
    }

    char esc = raw[++i];
    switch (esc) {
      case 'n':
        out += '\n';
        break;
      case 'r':
        out += '\r';
        break;
      case 't':
        out += '\t';
        break;
      case 'b':
        out += '\b';
        break;
      case 'f':
        out += '\f';
        break;
      case 'v':
        out += '\v';
        break;
      case '\\':
        out += '\\';
        break;
      case '"':
        out += '"';
        break;
      case '0':
        {
          int value = 0;
          size_t digits = 0;
          while (digits < 3 && i < raw.size() && raw[i] >= '0' && raw[i] <= '7') {
            value = value * 8 + (raw[i] - '0');
            ++i;
            ++digits;
          }
          --i;
          out += static_cast<char>(value);
          break;
        }
      case 'x':
        {
          if (i + 2 < raw.size()) {
            int high = hexValue(raw[i + 1]);
            int low = hexValue(raw[i + 2]);
            if (high >= 0 && low >= 0) {
              out += static_cast<char>(high * 16 + low);
              i += 2;
              break;
            }
          }
          out += '\\';
          out += 'x';
          break;
        }
      default:
        out += '\\';
        out += esc;
        break;
    }
  }
  return out;
}

std::string Interpreter::stringify(const gecko::Value& value)
{
  return std::visit(gecko::overloaded{[](gecko::Nil) -> std::string {
                                        return "null";
                                      },
                                      [](bool b) -> std::string {
                                        return b ? "true" : "false";
                                      },
                                      [](double d) -> std::string {
                                        std::ostringstream oss;
                                        if (std::abs(d - std::round(d)) < 1e-9) {
                                          auto i = static_cast<int64_t>(d);
                                          if (std::abs(d - static_cast<double>(i)) < 1e-12) {
                                            oss << i;
                                          }
                                          else {
                                            oss << d;
                                          }
                                        }
                                        else {
                                          oss << d;
                                        }
                                        return oss.str();
                                      },
                                      [](const std::string& s) -> std::string {
                                        return s;
                                      },
                                      [this](const gecko::Array& vec) -> std::string {
                                        std::string out = "[";
                                        for (size_t i = 0; i < vec->size(); ++i) {
                                          if (i > 0) {
                                            out += ", ";
                                          }
                                          out += stringify((*vec)[i]);
                                        }
                                        out += "]";
                                        return out;
                                      },
                                      [this](const gecko::Object& env) -> std::string {
                                        std::string out = "{";
                                        auto names = env->getDeclaredNames();
                                        for (size_t i = 0; i < names.size(); ++i) {
                                          if (i > 0) {
                                            out += ", ";
                                          }
                                          out += names[i] + ": " + stringify(env->get(names[i]));
                                        }
                                        out += "}";
                                        return out;
                                      },
                                      [](const gecko::NativeFunction&) -> std::string {
                                        return "<fn>";
                                      },
                                      [](const std::shared_ptr<UserFunction>& func) -> std::string {
                                        return "<fn:" + func->name + ">";
                                      },
                                      [](const std::shared_ptr<UserClass>& klass) -> std::string {
                                        return "<class:" + klass->name + ">";
                                      },
                                      [](const std::shared_ptr<ClassInstance>&) -> std::string {
                                        return "<instance>";
                                      },
                                      [](const gecko::Promise&) -> std::string {
                                        return "<promise>";
                                      }},
                    value.data);
}

bool Interpreter::isTruthy(const gecko::Value& value)
{
  return std::visit(gecko::overloaded{[](gecko::Nil) {
                                        return false;
                                      },
                                      [](bool b) {
                                        return b;
                                      },
                                      [](double d) {
                                        return d != 0.0;
                                      },
                                      [](const std::string& s) {
                                        return !s.empty();
                                      },
                                      [](const auto&) {
                                        return true;
                                      }},
                    value.data);
}

std::string Interpreter::getType(const gecko::Value& value)
{
  return std::visit(gecko::overloaded{[](gecko::Nil) -> std::string {
                                        return "null";
                                      },
                                      [](bool) -> std::string {
                                        return "boolean";
                                      },
                                      [](double) -> std::string {
                                        return "number";
                                      },
                                      [](const std::string&) -> std::string {
                                        return "string";
                                      },
                                      [](const gecko::Array&) -> std::string {
                                        return "array";
                                      },
                                      [](const gecko::Object&) -> std::string {
                                        return "object";
                                      },
                                      [](const std::shared_ptr<ClassInstance>&) -> std::string {
                                        return "instance";
                                      },
                                      [](const std::shared_ptr<UserClass>&) -> std::string {
                                        return "class";
                                      },
                                      [](const gecko::NativeFunction&) -> std::string {
                                        return "function";
                                      },
                                      [](const std::shared_ptr<UserFunction>&) -> std::string {
                                        return "function";
                                      },
                                      [](const gecko::Promise&) -> std::string {
                                        return "promise";
                                      }},
                    value.data);
}
