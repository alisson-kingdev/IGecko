#include "JsonModule.h"

#include <fstream>
#include <nlohmann/json.hpp>

#include "../runtime/Runtime.h"

namespace module
{

namespace
{

nlohmann::json valueToJson(const gecko::Value& val)
{
  return std::visit(
      gecko::overloaded{[](gecko::Nil) -> nlohmann::json {
                          return nullptr;
                        },
                        [](bool b) -> nlohmann::json {
                          return b;
                        },
                        [](double d) -> nlohmann::json {
                          return d;
                        },
                        [](const std::string& s) -> nlohmann::json {
                          return s;
                        },
                        [](const gecko::Array& arr) -> nlohmann::json {
                          nlohmann::json j = nlohmann::json::array();
                          for (const auto& v : *arr) {
                            j.push_back(valueToJson(v));
                          }
                          return j;
                        },
                        [](const gecko::Object& obj) -> nlohmann::json {
                          nlohmann::json j = nlohmann::json::object();
                          auto names = obj->getDeclaredNames();
                          for (const auto& name : names) {
                            j[name] = valueToJson(obj->get(name));
                          }
                          return j;
                        },
                        [](const auto&) -> nlohmann::json {
                          throw std::runtime_error("json.stringify: unsupported type");
                        }},
      val.data);
}

gecko::Value jsonToValue(const nlohmann::json& j)
{
  switch (j.type()) {
    case nlohmann::json::value_t::null:
      return nullptr;
    case nlohmann::json::value_t::boolean:
      return j.get<bool>();
    case nlohmann::json::value_t::number_integer:
    case nlohmann::json::value_t::number_unsigned:
    case nlohmann::json::value_t::number_float:
      return j.get<double>();
    case nlohmann::json::value_t::string:
      return j.get<std::string>();
    case nlohmann::json::value_t::array:
      {
        auto arr = std::make_shared<std::vector<gecko::Value>>();
        for (const auto& elem : j) {
          arr->push_back(jsonToValue(elem));
        }
        return arr;
      }
    case nlohmann::json::value_t::object:
      {
        auto obj = std::make_shared<Environment>();
        for (auto it = j.begin(); it != j.end(); ++it) {
          obj->define(it.key(), jsonToValue(it.value()));
        }
        return obj;
      }
    default:
      throw std::runtime_error("json: unsupported JSON type");
  }
}

} // namespace

std::shared_ptr<Environment> createJsonModule(std::shared_ptr<Environment> globals)
{
  auto env = std::make_shared<Environment>(globals);
  env->setPackageName("json");

  env->define("parse",
              gecko::NativeFunction([](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("json.parse expects a string");
                }
                auto j = nlohmann::json::parse(args[0].as<std::string>(), nullptr, false);
                if (j.is_discarded()) {
                  throw std::runtime_error("json.parse: invalid JSON");
                }
                return jsonToValue(j);
              }));

  env->define("stringify",
              gecko::NativeFunction([](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty()) {
                  throw std::runtime_error("json.stringify expects a value");
                }
                return valueToJson(args[0]).dump();
              }));

  env->define("prettify",
              gecko::NativeFunction([](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty()) {
                  throw std::runtime_error("json.prettify expects a value");
                }
                int indent = 2;
                if (args.size() > 1 && args[1].isNumber()) {
                  indent = static_cast<int>(args[1].as<double>());
                }
                return valueToJson(args[0]).dump(indent);
              }));

  env->define("read",
              gecko::NativeFunction([](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("json.read expects a file path");
                }
                std::ifstream file(args[0].as<std::string>());
                if (!file.is_open()) {
                  throw std::runtime_error("json.read: cannot open file");
                }
                nlohmann::json j;
                file >> j;
                return jsonToValue(j);
              }));

  env->define("write",
              gecko::NativeFunction([](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString()) {
                  throw std::runtime_error("json.write expects a file path and a value");
                }
                auto json = valueToJson(args[1]);
                std::ofstream file(args[0].as<std::string>());
                if (!file.is_open()) {
                  throw std::runtime_error("json.write: cannot open file");
                }
                file << json.dump(2);
                return nullptr;
              }));

  env->exportName("parse");
  env->exportName("stringify");
  env->exportName("prettify");
  env->exportName("read");
  env->exportName("write");

  return env;
}

} // namespace module
