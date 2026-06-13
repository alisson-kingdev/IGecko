#ifndef CLI_COMMANDS_CHECK_COMMAND_H
#define CLI_COMMANDS_CHECK_COMMAND_H

#include <string>
#include <vector>

namespace cli
{
class CheckCommand
{
public:
  static void execute(const std::string& filepath);
};
} // namespace cli

#endif
