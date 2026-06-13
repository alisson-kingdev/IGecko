#include "ModuleLoader.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "../../lexer/Lexer.h"
#include "../../parser/Parser.h"
#include "../../utils/Logger.h"
#include "../Interpreter.h"
#include "../environment/Environment.h"
#include "../runtime/Runtime.h"
#include "FsModule.h"
#include "HttpModule.h"
#include "JsonModule.h"

using json = nlohmann::json;

// Helper to check if a path is safe (resides within an allowed directory)
bool isPathSafe(const std::filesystem::path& path,
                const std::vector<std::filesystem::path>& allowedBases)
{
  std::filesystem::path canonicalPath;
  try {
    canonicalPath = std::filesystem::canonical(path);
  }
  catch (const std::exception& e) {
    return false;
  }

  for (const auto& base : allowedBases) {
    if (base.empty() || !std::filesystem::exists(base)) {
      continue;
    }
    std::filesystem::path canonicalBase;
    try {
      canonicalBase = std::filesystem::canonical(base);
    }
    catch (const std::exception& e) {
      continue;
    }

    auto [mismatch_base, mismatch_path] =
        std::mismatch(canonicalBase.begin(), canonicalBase.end(), canonicalPath.begin());
    if (mismatch_base == canonicalBase.end()) {
      return true;
    }
  }
  return false;
}

// Helper to identify project root
std::filesystem::path getProjectRoot(const std::string& sourceFile)
{
  std::filesystem::path currentPath =
      std::filesystem::absolute(std::filesystem::path(sourceFile).parent_path());
  std::filesystem::path root = currentPath;
  while (currentPath.has_parent_path()) {
    if (std::filesystem::exists(currentPath / ".git")
        || std::filesystem::exists(currentPath / "CMakeLists.txt")) {
      root = currentPath;
      break;
    }
    currentPath = currentPath.parent_path();
  }
  return root;
}

// Helper function to get the default stdlib path based on OS
std::filesystem::path getGeckoStdlibPath()
{
  // Fallback to OS-specific default paths
#ifdef _WIN32
  return std::filesystem::path("C:/ProgramData/gecko/stdlibs");
#else
  return std::filesystem::path("/opt/gecko/stdlibs");
#endif
}

// Helper function to load stdlib path from config
std::filesystem::path getConfiguredStdlibPath(const std::string& sourceFile)
{
  // Look for gecko.config.json in the source file's directory or parent
  // directories
  std::filesystem::path sourcePath(sourceFile);
  std::filesystem::path currentDir = sourcePath.parent_path();

  // Check current directory and up to 5 parent levels
  for (int i = 0; i < 5; i++) {
    std::filesystem::path configPath = currentDir / "gecko.config.json";
    if (std::filesystem::exists(configPath)) {
      try {
        std::ifstream configFile(configPath);
        json config;
        configFile >> config;
        if (config.contains("stdlibPath")) {
          std::string stdlibPathStr = config["stdlibPath"].get<std::string>();
          // If path is relative, make it relative to config file location
          std::filesystem::path stdlibPath(stdlibPathStr);
          if (!stdlibPath.is_absolute()) {
            stdlibPath = (currentDir / stdlibPath).lexically_normal();
          }
          return stdlibPath;
        }
      }
      catch (const std::exception&) {
        // Continue searching
      }
    }

    // Move up one directory
    if (currentDir == currentDir.root_path()) {
      break;
    }
    currentDir = currentDir.parent_path();
  }

  // Return default stdlib path if no config found
  return getGeckoStdlibPath();
}

// Helper function to load module path from config
std::filesystem::path getConfiguredModulePath(const std::string& sourceFile)
{
  std::filesystem::path sourcePath(sourceFile);
  std::filesystem::path currentDir = sourcePath.parent_path();

  for (int i = 0; i < 5; i++) {
    std::filesystem::path configPath = currentDir / "gecko.config.json";
    if (std::filesystem::exists(configPath)) {
      try {
        std::ifstream configFile(configPath);
        json config;
        configFile >> config;
        std::string moduleDirStr = config.value("moduleDir", "./modules");
        std::filesystem::path moduleDir(moduleDirStr);
        if (!moduleDir.is_absolute()) {
          moduleDir = (currentDir / moduleDir).lexically_normal();
        }
        return moduleDir;
      }
      catch (...) {
      }
    }
    if (currentDir == currentDir.root_path()) {
      break;
    }
    currentDir = currentDir.parent_path();
  }
  return std::filesystem::path();
}

ModuleLoader::ModuleLoader(std::shared_ptr<Environment> globals_,
                           std::shared_ptr<ModuleCache> moduleCache_,
                           const std::string& sourceFile_)
    : globals(std::move(globals_)), moduleCache(std::move(moduleCache_)), sourceFile(sourceFile_)
{}

std::shared_ptr<Environment> ModuleLoader::loadModule(const std::string& modulePath)
{
  // Native modules check
  if (modulePath == "fs") {
    return module::createFsModule(globals);
  }
  if (modulePath == "http") {
    return module::createHttpModule(globals);
  }
  if (modulePath == "json") {
    return module::createJsonModule(globals);
  }

  std::filesystem::path path(modulePath);
  std::vector<std::string> extensions = {".gk", ""};

  std::filesystem::path resolvedPath;
  bool found = false;

  // First, try in GPM modules directory
  {
    std::filesystem::path moduleBase = getConfiguredModulePath(sourceFile);
    if (!moduleBase.empty()) {
      auto modPath = (moduleBase / modulePath / "index.gk").lexically_normal();
      if (std::filesystem::exists(modPath)) {
        resolvedPath = std::filesystem::absolute(modPath).lexically_normal();
        found = true;
      }
    }
  }

  // Then, try in configured stdlib location
  if (!found) {
    std::filesystem::path stdlibBase = getConfiguredStdlibPath(sourceFile);
    auto stdLibPath = (stdlibBase / modulePath / "index.gk").lexically_normal();
    if (std::filesystem::exists(stdLibPath)) {
      resolvedPath = std::filesystem::absolute(stdLibPath).lexically_normal();
      found = true;
    }
  }

  // If not found, try relative to source file
  if (!found) {
    for (const auto& ext : extensions) {
      std::filesystem::path candidate = path;
      if (!candidate.has_extension() && !ext.empty()) {
        candidate += ext;
      }

      if (candidate.is_relative()) {
        auto sourceDir = std::filesystem::path(sourceFile).parent_path();
        candidate = std::filesystem::absolute(sourceDir / candidate).lexically_normal();
      }
      else {
        candidate = std::filesystem::absolute(candidate).lexically_normal();
      }

      if (std::filesystem::exists(candidate)) {
        resolvedPath = candidate;
        found = true;
        break;
      }
    }
  }

  // If still not found, try legacy locations (for backward compatibility)
  if (!found) {
    std::vector<std::filesystem::path> roots;
    roots.push_back(std::filesystem::current_path());

    auto sourceDir = std::filesystem::path(sourceFile).parent_path();
    if (!sourceDir.empty()) {
      if (sourceDir.is_relative()) {
        sourceDir = std::filesystem::absolute(sourceDir).lexically_normal();
      }
      roots.push_back(sourceDir);
    }

    for (auto root : roots) {
      if (root.is_relative()) {
        root = std::filesystem::absolute(root).lexically_normal();
      }

      while (!root.empty()) {
        auto stdLibPath = (root / "src" / "stdlib" / modulePath / "index.gk").lexically_normal();
        if (std::filesystem::exists(stdLibPath)) {
          resolvedPath = std::filesystem::absolute(stdLibPath).lexically_normal();
          found = true;
          break;
        }

        auto parent = root.parent_path();
        if (parent == root) {
          break;
        }
        root = parent;
      }

      if (found) {
        break;
      }
    }
  }

  if (!found) {
    throw std::runtime_error("ModuleNotFoundError: No module named '" + modulePath + "'");
  }

  std::vector<std::filesystem::path> allowedBases;
  std::filesystem::path projectRoot = std::filesystem::current_path();
  allowedBases.push_back(projectRoot);
  allowedBases.push_back(projectRoot / "src" / "stdlib");
  allowedBases.push_back(getConfiguredStdlibPath(sourceFile));

  if (!isPathSafe(resolvedPath, allowedBases)) {
    throw std::runtime_error(
        "SecurityError: Attempted to access file outside of allowed project root or stdlib: "
        + resolvedPath.string());
  }

  std::string fullPath = resolvedPath.string();
  if (moduleCache->find(fullPath) != moduleCache->end()) {
    return (*moduleCache)[fullPath];
  }

  std::ifstream file(fullPath);
  if (!file.is_open()) {
    throw std::runtime_error("ModuleNotFoundError: No module named '" + modulePath + "'");
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string source = buffer.str();

  Logger::setCurrentFile(fullPath);
  Lexer lexer(source, fullPath);
  auto tokens = lexer.tokenize();

  Parser parser(tokens);
  auto statements = parser.parse();

  auto moduleEnv = std::make_shared<Environment>(globals);

  Interpreter moduleInterpreter(fullPath);
  moduleInterpreter.globals = globals;
  moduleInterpreter.environment = moduleEnv;
  moduleInterpreter.moduleCache = moduleCache;

  Logger::pushFrame("<module>", SourceLocation(fullPath, 1, 1));
  try {
    moduleInterpreter.executeStatements(statements);
  }
  catch (const std::exception& e) {
    auto trace = Logger::captureTrace();
    Logger::popFrame();
    if (dynamic_cast<const RuntimeError*>(&e)) {
      throw;
    }
    throw RuntimeError(e.what(), std::move(trace));
  }
  catch (...) {
    Logger::popFrame();
    throw;
  }
  Logger::popFrame();

  (*moduleCache)[fullPath] = moduleEnv;
  return moduleEnv;
}
