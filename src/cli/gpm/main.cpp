#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../../utils/ProcessUtils.h"
#include "../../utils/Version.h"
#include "Config.h"
#include "Installer.h"

using namespace gpm;
namespace fs = std::filesystem;

void showHelp()
{
  json meta = Config::getGpmMetadata();
  std::cout << meta.value("description", "Gecko Package Manager") << " (v"
            << meta.value("version", "0.1.0") << ")\n"
            << "Gecko Engine: v" << gecko::getVersion() << "\n"
            << "Usage:\n"
            << "  gpm init                Initialize a new project\n"
            << "  gpm run <script>        Run a script defined in gecko.config.json\n"
            << "  gpm install|i <pkg>     Install a package from official registry\n"
            << "  gpm install|i -g <repo> Install a package from GitHub (user/repo[:branch])\n"
            << "  gpm remove|rm <pkg>     Remove a package\n"
            << "  gpm list|ls             List installed packages\n"
            << "  gpm update              Update all packages\n"
            << "  gpm -v|--version        Show version\n"
            << "  gpm help                Show this help\n";
}

std::string prompt(const std::string& field, const std::string& defaultValue)
{
  std::cout << field << ": (" << defaultValue << ") ";
  std::string input;
  std::getline(std::cin, input);
  return input.empty() ? defaultValue : input;
}

int main(int argc, char* argv[])
{
  try {
    if (argc < 2) {
      showHelp();
      return 0;
    }

    std::string cmd = argv[1];

    if (cmd == "-v" || cmd == "--version") {
      json meta = Config::getGpmMetadata();
      std::cout << meta.value("name", "gpm") << " version " << meta.value("version", "0.1.0")
                << std::endl;
      return 0;
    }

    if (cmd == "help") {
      showHelp();
      return 0;
    }

    fs::path root = Config::findProjectRoot();

    if (cmd == "init") {
      fs::path configPath = fs::current_path() / "gecko.config.json";
      if (fs::exists(configPath)) {
        std::cout << "Project already initialized." << std::endl;
        return 1;
      }

      std::cout << "This utility will walk you through creating a gecko.config.json file."
                << std::endl;
      std::cout << "Press ^C at any time to quit." << std::endl;

      std::string dirName = fs::current_path().filename().string();
      std::string name = prompt("package name", dirName);
      std::string version = prompt("version", "1.0.0");
      std::string description = prompt("description", "");
      std::string entryPoint = prompt("entry point", "main.gk");
      std::string testCmd = prompt("test command", "echo 'Test completed!'");
      std::string repo = prompt("git repository", "");
      std::string keywords = prompt("keywords", "");
      std::string author = prompt("author", "");
      std::string license = prompt("license", "ISC");

      // Constructing JSON using ordered_json (aliased as json)
      json config;
      config["name"] = name;
      config["version"] = version;
      config["description"] = description;
      config["main"] = entryPoint;
      config["scripts"] = json{{"test", testCmd}};
      if (!repo.empty()) {
        config["repository"] = repo;
      }

      json kwArr = json::array();
      if (!keywords.empty()) {
        size_t start = 0;
        size_t end = keywords.find(",");
        while (end != std::string::npos) {
          std::string k = keywords.substr(start, end - start);
          k.erase(0, k.find_first_not_of(" "));
          k.erase(k.find_last_not_of(" ") + 1);
          kwArr.push_back(k);
          start = end + 1;
          end = keywords.find(",", start);
        }
        std::string lastK = keywords.substr(start);
        lastK.erase(0, lastK.find_first_not_of(" "));
        lastK.erase(lastK.find_last_not_of(" ") + 1);
        if (!lastK.empty()) {
          kwArr.push_back(lastK);
        }
      }
      config["keywords"] = kwArr;

      config["author"] = author;
      config["license"] = license;
      config["dependencies"] = json::object();

      std::cout << "\nAbout to write to " << configPath << ":\n" << config.dump(2) << std::endl;
      std::string ok = prompt("Is this OK?", "yes");
      if (ok != "yes" && ok != "y") {
        std::cout << "Aborted." << std::endl;
        return 0;
      }

      Config::saveConfig(fs::current_path(), config);
      json lock = {{"version", 1}, {"packages", json::object()}};
      Config::saveLock(fs::current_path(), lock);

      std::ofstream mainFile(entryPoint);
      mainFile << "package main;\n\nfunc main() {\n    print(\"Hello from " << name
               << "!\");\n}\n\nmain();" << std::endl;
      std::cout << "✓ Initialized Gecko project: " << name << std::endl;
    }
    else if (cmd == "run") {
      if (argc < 3) {
        throw std::runtime_error("Missing script name");
      }
      std::string scriptName = argv[2];
      json config = Config::loadConfig(root);
      if (!config.contains("scripts") || !config["scripts"].contains(scriptName)) {
        throw std::runtime_error("Script not found: " + scriptName);
      }

      std::string command = config["scripts"][scriptName];
      std::cout << "SECURITY WARNING: GPM is about to execute the following shell command from "
                   "gecko.config.json:"
                << std::endl;
      std::cout << "  " << command << std::endl;
      std::string ok = prompt("Do you want to proceed? (yes/no)", "no");
      if (ok != "yes" && ok != "y") {
        std::cout << "Aborted." << std::endl;
        return 0;
      }

      std::cout << "> " << command << std::endl;

      utils::CommandResult result = utils::runCommand({"/bin/sh", "-c", command});
      if (result.exitCode != 0) {
        std::cerr << "Script failed with exit code " << result.exitCode << std::endl;
        return result.exitCode;
      }
      std::cout << result.output;
      return 0;
    }
    else if (cmd == "install" || cmd == "i") {
      if (argc < 3) {
        throw std::runtime_error("Missing package name");
      }
      std::string pkg = argv[2];
      if (pkg == "-g") {
        if (argc < 4) {
          throw std::runtime_error("Missing repository name");
        }
        Installer::install(root, argv[3], true);
      }
      else {
        Installer::install(root, pkg, false);
      }
    }
    else if (cmd == "remove" || cmd == "rm") {
      if (argc < 3) {
        throw std::runtime_error("Missing package name");
      }
      Installer::uninstall(root, argv[2]);
    }
    else if (cmd == "list" || cmd == "ls") {
      json lock = Config::loadLock(root);
      if (!lock.contains("packages") || lock["packages"].empty()) {
        std::cout << "No packages installed." << std::endl;
      }
      else {
        std::cout << "Installed packages:" << std::endl;
        for (auto& [name, data] : lock["packages"].items()) {
          std::cout << "  " << name << " (" << data.value("source", "unknown") << ")" << std::endl;
        }
      }
    }
    else if (cmd == "update") {
      Installer::updateAll(root);
    }
    else {
      std::cerr << "Unknown command: " << cmd << std::endl;
      return 1;
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
