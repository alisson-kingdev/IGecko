#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <filesystem>
#include <limits.h>
#include <string>
#include <unistd.h>

/**
 * @file PathUtils.h
 * @brief Utilities for path manipulation and executable location resolution.
 */

#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <filesystem>
#include <limits.h>
#include <string>
#include <unistd.h>

namespace utils
{

/**
 * @brief Retrieves the directory path where the current executable is located.
 *
 * @return std::filesystem::path Path to the executable's directory.
 */
inline std::filesystem::path getExecutablePath()
{
#ifdef __linux__
  try {
    return std::filesystem::canonical("/proc/self/exe").parent_path();
  }
  catch (const std::filesystem::filesystem_error&) {
    // Fallback if canonical fails
  }
#endif
  // Fallback for other platforms or if /proc/self/exe is not available
  char result[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
  if (count != -1) {
    return std::filesystem::path(std::string(result, count)).parent_path();
  }
  return std::filesystem::current_path();
}
} // namespace utils

#endif // namespace utils

#endif
