/**
 * @file ProcessUtils.h
 * @brief Utilities for process management and command execution.
 */

#ifndef PROCESS_UTILS_H
#define PROCESS_UTILS_H

#include <filesystem>
#include <string>
#include <unistd.h>
#include <vector>

namespace utils
{

/**
 * @brief RAII wrapper for file descriptors to ensure automatic cleanup.
 */
class ScopedFd
{
public:
  explicit ScopedFd(int fd) : fd_(fd) {}

  ~ScopedFd()
  {
    if (fd_ != -1) {
      close(fd_);
    }
  }

  int get() const
  {
    return fd_;
  }

  // Disable copying to prevent double-closing
  ScopedFd(const ScopedFd&) = delete;
  ScopedFd& operator=(const ScopedFd&) = delete;

private:
  int fd_;
};

/**
 * @brief Result of a command execution.
 */
struct CommandResult
{
  int exitCode;
  std::string output;
};

/**
 * @brief Executes a system command.
 *
 * @param args Vector of command arguments.
 * @param workingDir Optional working directory.
 * @return CommandResult containing exit code and stdout.
 */
CommandResult runCommand(const std::vector<std::string>& args,
                         const std::filesystem::path& workingDir = "");

} // namespace utils

#endif // PROCESS_UTILS_H
