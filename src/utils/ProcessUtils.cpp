/**
 * @file ProcessUtils.cpp
 * @brief Implementation of process management utilities.
 */

#include "ProcessUtils.h"

#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace utils
{

CommandResult runCommand(const std::vector<std::string>& args,
                         const std::filesystem::path& workingDir)
{
  int pipefd[2];
  if (pipe(pipefd) == -1) {
    throw std::runtime_error("Failed to create pipe");
  }

  // Use RAII wrappers for file descriptors
  ScopedFd readFd(pipefd[0]);
  ScopedFd writeFd(pipefd[1]);

  pid_t pid = fork();
  if (pid == -1) {
    throw std::runtime_error("Failed to fork process");
  }

  if (pid == 0) { // Child process
    if (!workingDir.empty()) {
      if (chdir(workingDir.c_str()) == -1) {
        exit(EXIT_FAILURE);
      }
    }

    // Child needs to close the read end of the pipe
    // The write end will be duplicated to STDOUT/STDERR,
    // then the original fd will be closed by ScopedFd.
    dup2(writeFd.get(), STDOUT_FILENO);
    dup2(writeFd.get(), STDERR_FILENO);

    std::vector<char*> cArgs;
    for (const auto& arg : args) {
      cArgs.push_back(const_cast<char*>(arg.c_str()));
    }
    cArgs.push_back(nullptr);

    if (execvp(cArgs[0], cArgs.data()) == -1) {
      std::cerr << "Error: execvp failed: " << std::strerror(errno) << std::endl;
      exit(EXIT_FAILURE);
    }
    exit(EXIT_FAILURE);
  }
  else { // Parent process
    // ScopedFd writeFd will close pipefd[1] here

    std::string output;
    std::array<char, 4096> buffer;
    ssize_t bytesRead;
    while ((bytesRead = read(readFd.get(), buffer.data(), buffer.size())) > 0) {
      output.append(buffer.data(), static_cast<std::size_t>(bytesRead));
    }
    // ScopedFd readFd will close pipefd[0] here

    int status;
    waitpid(pid, &status, 0);
    return {WEXITSTATUS(status), output};
  }
}

} // namespace utils
