#ifndef GPM_INSTALLER_H
#define GPM_INSTALLER_H

#include <filesystem>
#include <string>

namespace gpm
{
namespace fs = std::filesystem;

class Installer
{
public:
  static void install(const fs::path& root, const std::string& package, bool isGit = false);
  static void uninstall(const fs::path& root, const std::string& packageName);
  static void updateAll(const fs::path& root);

private:
  static void
  downloadGit(const std::string& repo, const fs::path& dest, const std::string& branch = "");
  static std::string getCommitHash(const fs::path& modulePath);
};
} // namespace gpm

#endif
