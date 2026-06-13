#include "Installer.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <regex>
#include <string>

#include "Config.h"

namespace gpm
{

void Installer::install(const fs::path& root, const std::string& package, bool isGit)
{
  json config = Config::loadConfig(root);
  json lock = Config::loadLock(root);

  std::string moduleDirName = config.value("moduleDir", "./modules");
  fs::path modulesPath = root / moduleDirName;
  if (!fs::exists(modulesPath)) {
    fs::create_directories(modulesPath);
  }

  std::string repoUrl = package;
  std::string branch = "";
  std::string moduleName;
  std::string fullUrl;

  if (isGit) {
    if (fs::exists(package)) {
      moduleName = fs::path(package).filename().string();
      repoUrl = fs::absolute(package).string();
      fullUrl = repoUrl;
      isGit = false;
    }
    else {
      size_t colon = package.find(':');
      if (colon != std::string::npos) {
        repoUrl = package.substr(0, colon);
        branch = package.substr(colon + 1);
      }
      moduleName = repoUrl.substr(repoUrl.find_last_of('/') + 1);
      if (moduleName.find(".git") != std::string::npos) {
        moduleName = moduleName.substr(0, moduleName.size() - 4);
      }
      fullUrl = "https://github.com/" + repoUrl;
    }
  }
  else {
    repoUrl = "https://github.com/TheKingKoders/" + package;
    fullUrl = repoUrl;
    moduleName = package;
  }

  fs::path dest = modulesPath / moduleName;
  if (fs::exists(dest)) {
    std::cout << "Module " << moduleName << " already installed. Updating..." << std::endl;
    fs::remove_all(dest);
  }

  if (!isGit && fs::exists(repoUrl)) {
    fs::copy(repoUrl, dest, fs::copy_options::recursive);
  }
  else {
    if (repoUrl.find("http") == 0) {
      fullUrl = repoUrl;
    }
    downloadGit(fullUrl, dest, branch);
  }

  std::string commit = getCommitHash(dest);

  config["dependencies"][moduleName] = package;
  Config::saveConfig(root, config);

  lock["packages"][moduleName] = json::object({{"source", isGit ? "github" : "official"},
                                               {"url", fullUrl},
                                               {"branch", branch.empty() ? "main" : branch},
                                               {"commit", commit}});
  Config::saveLock(root, lock);

  std::cout << "✓ Installed " << moduleName << " (commit: " << commit.substr(0, 7) << ")"
            << std::endl;
}

void Installer::uninstall(const fs::path& root, const std::string& packageName)
{
  json config = Config::loadConfig(root);
  json lock = Config::loadLock(root);

  std::string moduleDirName = config.value("moduleDir", "./modules");
  fs::path dest = root / moduleDirName / packageName;

  if (fs::exists(dest)) {
    fs::remove_all(dest);
  }

  if (config["dependencies"].contains(packageName)) {
    config["dependencies"].erase(packageName);
  }
  if (lock["packages"].contains(packageName)) {
    lock["packages"].erase(packageName);
  }

  Config::saveConfig(root, config);
  Config::saveLock(root, lock);

  std::cout << "✓ Removed " << packageName << std::endl;
}

void Installer::updateAll(const fs::path& root)
{
  json config = Config::loadConfig(root);
  if (!config.contains("dependencies")) {
    return;
  }

  for (auto& [name, spec] : config["dependencies"].items()) {
    bool isGit = spec.get<std::string>().find('/') != std::string::npos;
    install(root, spec, isGit);
  }
}

void Installer::downloadGit(const std::string& url, const fs::path& dest, const std::string& branch)
{
  std::regex valid_url("^[a-zA-Z0-9._/:\\-]+$");
  std::regex valid_branch("^[a-zA-Z0-9._\\-]+$");

  if (!std::regex_match(url, valid_url)) {
    throw std::runtime_error("Invalid URL format");
  }
  if (!branch.empty() && !std::regex_match(branch, valid_branch)) {
    throw std::runtime_error("Invalid branch name");
  }

  std::string cmd = "git clone --depth 1 ";
  if (!branch.empty()) {
    cmd += "-b " + branch + " ";
  }
  cmd += url + " " + dest.string() + " > /dev/null 2>&1";

  int res = std::system(cmd.c_str());
  if (res != 0) {
    throw std::runtime_error("Failed to download module from " + url);
  }
}

std::string Installer::getCommitHash(const fs::path& modulePath)
{
  if (!fs::exists(modulePath / ".git")) {
    return "local-version";
  }

  std::string path_str = modulePath.string();
  std::replace(path_str.begin(), path_str.end(), '"', ' ');

  std::string cmd = "cd \"" + path_str + "\" && git rev-parse HEAD";
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return "unknown";
  }
  char buffer[128];
  std::string result = "";
  while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
    result += buffer;
  }
  pclose(pipe);
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

} // namespace gpm
