#ifndef GPM_CONFIG_H
#define GPM_CONFIG_H

#include <filesystem>
#include <nlohmann/json.hpp>

namespace gpm
{
namespace fs = std::filesystem;
using json = nlohmann::ordered_json;

class Config
{
public:
  static fs::path findProjectRoot();
  static json loadConfig(const fs::path& root);
  static void saveConfig(const fs::path& root, const json& config);
  static json loadLock(const fs::path& root);
  static void saveLock(const fs::path& root, const json& lock);
  static json getGpmMetadata();
};
} // namespace gpm

#endif
