#include <iostream>

#include "../Interpreter.h"

void Interpreter::defineBuiltins()
{
  globals->define(
      "print",
      gecko::NativeFunction([this](const std::vector<gecko::Value>& args) -> gecko::Value {
        for (size_t i = 0; i < args.size(); ++i) {
          if (i > 0) {
            (*outStream) << " ";
          }
          (*outStream) << stringify(args[i]);
        }
        (*outStream) << std::endl;
        return nullptr;
      }));

  globals->define(
      "typeOf",
      gecko::NativeFunction([this](const std::vector<gecko::Value>& args) -> gecko::Value {
        if (args.empty()) {
          return std::string("undefined");
        }
        return getType(args[0]);
      }));

  globals->define("Object",
                  gecko::NativeFunction([](const std::vector<gecko::Value>& args) -> gecko::Value {
                    if (args.empty()) {
                      return std::make_shared<Environment>();
                    }
                    return args[0];
                  }));

  globals->define("input",
                  gecko::NativeFunction([](const std::vector<gecko::Value>& args) -> gecko::Value {
                    if (args.empty() || !args[0].isString()) {
                      throw std::runtime_error("input() expects a string prompt");
                    }
                    std::cout << args[0].as<std::string>() << std::flush;
                    std::string line;
                    if (!std::getline(std::cin, line)) {
                      return std::string("");
                    }
                    return line;
                  }));
}
