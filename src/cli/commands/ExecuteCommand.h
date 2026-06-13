#ifndef CLI_COMMANDS_EXECUTE_COMMAND_H
#define CLI_COMMANDS_EXECUTE_COMMAND_H

#include <string>

namespace cli
{
class ExecuteCommand
{
public:
  static void execute(const std::string& filepath);
};
} // namespace cli

#endif
