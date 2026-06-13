#include "Printer.h"

namespace
{
const std::string RESET = "\033[0m";
const std::string RED = "\033[0;31m";
const std::string GREEN = "\033[0;32m";
const std::string BLUE = "\033[0;34m";
const std::string BOLD = "\033[1m";
} // namespace

namespace cli
{
std::string Printer::red(const std::string& text)
{
  return RED + text + RESET;
}

std::string Printer::green(const std::string& text)
{
  return GREEN + text + RESET;
}

std::string Printer::blue(const std::string& text)
{
  return BLUE + text + RESET;
}

std::string Printer::bold(const std::string& text)
{
  return BOLD + text + RESET;
}

std::string Printer::success(const std::string& message)
{
  return green("[√] ") + message;
}

std::string Printer::error(const std::string& message)
{
  return red("[x] ") + message;
}
} // namespace cli
