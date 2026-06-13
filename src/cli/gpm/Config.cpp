#include "Config.h"

#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace gpm
{

fs::path Config::findProjectRoot()
{
  fs::path current = fs::current_path();
  for (int i = 0; i < 5; ++i) {
    if (fs::exists(current / "gecko.config.json")) {
      return current;
    }
    if (current == current.root_path()) {
      break;
    }
    current = current.parent_path();
  }
  return fs::current_path();
}

json Config::loadConfig(const fs::path& root)
{
  fs::path path = root / "gecko.config.json";
  if (!fs::exists(path)) {
    throw std::runtime_error("gecko.config.json not found");
  }
  std::ifstream file(path);
  json j;
  file >> j;
  return j;
}

void Config::saveConfig(const fs::path& root, const json& config)
{
  std::ofstream file(root / "gecko.config.json");
  file << std::setw(2) << config << std::endl;
}

json Config::loadLock(const fs::path& root)
{
  fs::path path = root / "gecko-lock.json";
  if (!fs::exists(path)) {
    return json({{"version", 1}, {"packages", json::object()}});
  }
  std::ifstream file(path);
  json j;
  file >> j;
  return j;
}

void Config::saveLock(const fs::path& root, const json& lock)
{
  std::ofstream file(root / "gecko-lock.json");
  file << std::setw(2) << lock << std::endl;
}

#include <libgen.h>
#include <limits.h>
#include <unistd.h>

json Config::getGpmMetadata()
{
  char result[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
  std::string execPath = std::string(result, (count > 0) ? static_cast<size_t>(count) : 0);
  fs::path binaryDir = fs::path(execPath).parent_path();

  fs::path path = binaryDir / "gpm.json";

  if (!fs::exists(path)) {
    // Fallback for development/install path
    path = "/data/data/com.termux/files/usr/share/igecko/gpm.json";
  }

  if (!fs::exists(path)) {
    return json({{"name", "gpm"}, {"version", "0.1.0"}, {"description", "Gecko Package Manager"}});
  }

  std::ifstream file(path);
  json j;
  file >> j;
  return j;
}

} // namespace gpm
