#ifndef CLI_UI_PRINTER_H
#define CLI_UI_PRINTER_H

#include <iostream>
#include <string>

namespace cli
{
class Printer
{
public:
  static std::string red(const std::string& text);
  static std::string green(const std::string& text);
  static std::string blue(const std::string& text);
  static std::string bold(const std::string& text);
  static std::string success(const std::string& message);
  static std::string error(const std::string& message);
};
} // namespace cli

#endif // CLI_UI_PRINTER_H
