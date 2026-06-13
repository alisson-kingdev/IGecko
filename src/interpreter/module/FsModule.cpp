#include "FsModule.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "../runtime/Runtime.h"

namespace module
{

namespace fs = std::filesystem;

namespace
{

std::string readFileContents(const fs::path& path)
{
  std::ifstream file(path, std::ios::in | std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to open file: " + path.string());
  }
  std::ostringstream buf;
  buf << file.rdbuf();
  return buf.str();
}

void writeFileContents(const fs::path& path, const std::string& data, bool append)
{
  auto flags = std::ios::out | std::ios::binary;
  if (append) flags |= std::ios::app;
  std::ofstream file(path, flags);
  if (!file) {
    throw std::runtime_error("Failed to write file: " + path.string());
  }
  file.write(data.data(), static_cast<std::streamsize>(data.size()));
}

std::string resolvePath(const std::string& sandbox, const std::string& userPath)
{
  fs::path p = fs::path(userPath);
  if (p.is_relative()) {
    p = fs::path(sandbox) / p;
  }
  p = p.lexically_normal();
  if (!p.string().starts_with(sandbox + "/") && p.string() != sandbox) {
    throw std::runtime_error("Path escapes sandbox: " + userPath);
  }
  return p.string();
}

nlohmann::json valueToJson(const gecko::Value& val)
{
  if (val.isNil()) return nullptr;
  if (val.isBool()) return val.as<bool>();
  if (val.isNumber()) return val.as<double>();
  if (val.isString()) return val.as<std::string>();
  if (val.isArray()) {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& v : *val.as<gecko::Array>()) {
      j.push_back(valueToJson(v));
    }
    return j;
  }
  if (val.isObject()) {
    nlohmann::json j = nlohmann::json::object();
    for (const auto& name : val.as<gecko::Object>()->getDeclaredNames()) {
      j[name] = valueToJson(val.as<gecko::Object>()->get(name));
    }
    return j;
  }
  return nullptr;
}

gecko::Value jsonToValue(const nlohmann::json& j)
{
  switch (j.type()) {
    case nlohmann::json::value_t::null:
      return nullptr;
    case nlohmann::json::value_t::boolean:
      return j.get<bool>();
    case nlohmann::json::value_t::number_integer:
    case nlohmann::json::value_t::number_unsigned:
    case nlohmann::json::value_t::number_float:
      return j.get<double>();
    case nlohmann::json::value_t::string:
      return j.get<std::string>();
    case nlohmann::json::value_t::array: {
      auto arr = std::make_shared<std::vector<gecko::Value>>();
      for (const auto& elem : j) {
        arr->push_back(jsonToValue(elem));
      }
      return arr;
    }
    case nlohmann::json::value_t::object: {
      auto obj = std::make_shared<Environment>();
      for (auto it = j.begin(); it != j.end(); ++it) {
        obj->define(it.key(), jsonToValue(it.value()));
      }
      return obj;
    }
    default:
      return nullptr;
  }
}

} // namespace

std::shared_ptr<Environment> createFsModule(std::shared_ptr<Environment> globals)
{
  auto env = std::make_shared<Environment>(globals);
  env->setPackageName("fs");

  auto sandboxRoot = std::make_shared<std::string>(fs::current_path().string());

  // ── Read ──

  env->define("read",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.read(path) expects a string path");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                return readFileContents(path);
              }));

  env->define("readBytes",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.readBytes(path) expects a string path");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                return readFileContents(path);
              }));

  env->define("readLines",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.readLines(path) expects a string path");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                auto content = readFileContents(path);
                auto arr = std::make_shared<std::vector<gecko::Value>>();
                std::istringstream stream(content);
                std::string line;
                while (std::getline(stream, line)) {
                  arr->push_back(line);
                }
                return arr;
              }));

  env->define("readJson",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.readJson(path) expects a string path");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                auto content = readFileContents(path);
                auto j = nlohmann::json::parse(content, nullptr, false);
                if (j.is_discarded()) {
                  throw std::runtime_error("fs.readJson: invalid JSON in " + path);
                }
                return jsonToValue(j);
              }));

  // ── Write ──

  env->define("write",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString()) {
                  throw std::runtime_error("fs.write(path, data) expects a string path and data");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                std::string data = args[1].isString() ? args[1].as<std::string>() : "";
                writeFileContents(path, data, false);
                return nullptr;
              }));

  env->define("writeBytes",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString()) {
                  throw std::runtime_error("fs.writeBytes(path, data) expects a string path and data");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                std::string data = args[1].isString() ? args[1].as<std::string>() : "";
                writeFileContents(path, data, false);
                return nullptr;
              }));

  env->define("append",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString()) {
                  throw std::runtime_error("fs.append(path, data) expects a string path and data");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                std::string data = args[1].isString() ? args[1].as<std::string>() : "";
                writeFileContents(path, data, true);
                return nullptr;
              }));

  env->define("writeJson",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString()) {
                  throw std::runtime_error("fs.writeJson(path, value) expects a string path and a value");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                auto j = valueToJson(args[1]);
                writeFileContents(path, j.dump(2), false);
                return nullptr;
              }));

  // ── Directory ──

  env->define("mkdir",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.mkdir(path) expects a string path");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                fs::create_directory(path);
                return nullptr;
              }));

  env->define("mkdirp",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.mkdirp(path) expects a string path");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                fs::create_directories(path);
                return nullptr;
              }));

  env->define("rmdir",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.rmdir(path) expects a string path");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                fs::remove(path);
                return nullptr;
              }));

  env->define("rmrf",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.rmrf(path) expects a string path");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                fs::remove_all(path);
                return nullptr;
              }));

  env->define("ls",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                std::string dir = args.empty() || !args[0].isString() ? "." : args[0].as<std::string>();
                auto path = resolvePath(*sandboxRoot, dir);
                auto arr = std::make_shared<std::vector<gecko::Value>>();
                for (const auto& entry : fs::directory_iterator(path)) {
                  arr->push_back(entry.path().filename().string());
                }
                return arr;
              }));

  env->define("lsFiles",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                std::string dir = args.empty() || !args[0].isString() ? "." : args[0].as<std::string>();
                auto path = resolvePath(*sandboxRoot, dir);
                auto arr = std::make_shared<std::vector<gecko::Value>>();
                for (const auto& entry : fs::directory_iterator(path)) {
                  if (entry.is_regular_file()) {
                    arr->push_back(entry.path().filename().string());
                  }
                }
                return arr;
              }));

  env->define("lsDirs",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                std::string dir = args.empty() || !args[0].isString() ? "." : args[0].as<std::string>();
                auto path = resolvePath(*sandboxRoot, dir);
                auto arr = std::make_shared<std::vector<gecko::Value>>();
                for (const auto& entry : fs::directory_iterator(path)) {
                  if (entry.is_directory()) {
                    arr->push_back(entry.path().filename().string());
                  }
                }
                return arr;
              }));

  env->define("tmpdir",
              gecko::NativeFunction([](const std::vector<gecko::Value>&) -> gecko::Value {
                return fs::temp_directory_path().string();
              }));

  env->define("cwd",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>&) -> gecko::Value {
                return *sandboxRoot;
              }));

  // ── File Info ──

  env->define("exists",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.exists(path) expects a string path");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                return fs::exists(path);
              }));

  env->define("isFile",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.isFile(path) expects a string path");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                return fs::is_regular_file(path);
              }));

  env->define("isDir",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.isDir(path) expects a string path");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                return fs::is_directory(path);
              }));

  env->define("isSymlink",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.isSymlink(path) expects a string path");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                return fs::is_symlink(path);
              }));

  env->define("size",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.size(path) expects a string path");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                return static_cast<double>(fs::file_size(path));
              }));

  env->define("mtime",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.mtime(path) expects a string path");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                auto ftime = fs::last_write_time(path);
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                return static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                               sctp.time_since_epoch())
                                               .count());
              }));

  env->define("extension",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.extension(path) expects a string path");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                return fs::path(path).extension().string();
              }));

  env->define("basename",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.basename(path) expects a string path");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                return fs::path(path).filename().string();
              }));

  env->define("dirname",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.dirname(path) expects a string path");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                return fs::path(path).parent_path().string();
              }));

  env->define("abs",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.abs(path) expects a string path");
                }
                return resolvePath(*sandboxRoot, args[0].as<std::string>());
              }));

  // ── File Manipulation ──

  env->define("copy",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                  throw std::runtime_error("fs.copy(src, dst) expects two string paths");
                }
                auto src = resolvePath(*sandboxRoot, args[0].as<std::string>());
                auto dst = resolvePath(*sandboxRoot, args[1].as<std::string>());
                fs::copy(src, dst, fs::copy_options::overwrite_existing);
                return nullptr;
              }));

  env->define("move",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                  throw std::runtime_error("fs.move(src, dst) expects two string paths");
                }
                auto src = resolvePath(*sandboxRoot, args[0].as<std::string>());
                auto dst = resolvePath(*sandboxRoot, args[1].as<std::string>());
                fs::rename(src, dst);
                return nullptr;
              }));

  env->define("remove",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.remove(path) expects a string path");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                fs::remove(path);
                return nullptr;
              }));

  env->define("link",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                  throw std::runtime_error("fs.link(src, dst) expects two string paths");
                }
                auto src = resolvePath(*sandboxRoot, args[0].as<std::string>());
                auto dst = resolvePath(*sandboxRoot, args[1].as<std::string>());
                fs::create_hard_link(src, dst);
                return nullptr;
              }));

  env->define("symlink",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                  throw std::runtime_error("fs.symlink(src, dst) expects two string paths");
                }
                auto src = resolvePath(*sandboxRoot, args[0].as<std::string>());
                auto dst = resolvePath(*sandboxRoot, args[1].as<std::string>());
                fs::create_symlink(src, dst);
                return nullptr;
              }));

  env->define("chmod",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString() || !args[1].isNumber()) {
                  throw std::runtime_error("fs.chmod(path, mode) expects a string path and a numeric mode");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                auto perm = static_cast<fs::perms>(static_cast<int>(args[1].as<double>()));
                fs::permissions(path, perm);
                return nullptr;
              }));

  env->define("stat",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.stat(path) expects a string path");
                }
                auto path = resolvePath(*sandboxRoot, args[0].as<std::string>());
                auto info = std::make_shared<Environment>();
                info->define("size", static_cast<double>(fs::file_size(path)));
                info->define("isFile", fs::is_regular_file(path));
                info->define("isDir", fs::is_directory(path));
                info->define("isSymlink", fs::is_symlink(path));
                info->define("exists", fs::exists(path));
                try {
                  auto ftime = fs::last_write_time(path);
                  auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                      ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                  info->define("mtime", static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                         sctp.time_since_epoch())
                                                             .count()));
                }
                catch (...) {
                  info->define("mtime", 0.0);
                }
                info->define("extension", fs::path(path).extension().string());
                info->define("basename", fs::path(path).filename().string());
                info->define("dirname", fs::path(path).parent_path().string());
                return info;
              }));

  // ── Path Operations ──

  env->define("join",
              gecko::NativeFunction([](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty()) {
                  return std::string();
                }
                fs::path result;
                for (const auto& arg : args) {
                  if (arg.isString()) {
                    result /= arg.as<std::string>();
                  }
                }
                return result.lexically_normal().string();
              }));

  env->define("normalize",
              gecko::NativeFunction([](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.normalize(path) expects a string path");
                }
                return fs::path(args[0].as<std::string>()).lexically_normal().string();
              }));

  env->define("resolve",
              gecko::NativeFunction([](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty()) {
                  return fs::current_path().string();
                }
                fs::path result;
                for (const auto& arg : args) {
                  if (arg.isString()) {
                    result /= arg.as<std::string>();
                  }
                }
                return fs::absolute(result).lexically_normal().string();
              }));

  env->define("relative",
              gecko::NativeFunction([](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                  throw std::runtime_error("fs.relative(from, to) expects two string paths");
                }
                auto from = fs::path(args[0].as<std::string>());
                auto to = fs::path(args[1].as<std::string>());
                return fs::relative(to, from).string();
              }));

  env->define("sep",
              gecko::NativeFunction([](const std::vector<gecko::Value>&) -> gecko::Value {
                return std::string(1, fs::path::preferred_separator);
              }));

  env->define("delimiter",
              gecko::NativeFunction([](const std::vector<gecko::Value>&) -> gecko::Value {
#ifdef _WIN32
                return std::string(";");
#else
                return std::string(":");
#endif
              }));

  // ── Sandbox Control ──

  env->define("chroot",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.chroot(path) expects a string path");
                }
                auto newRoot = fs::absolute(args[0].as<std::string>()).lexically_normal().string();
                *sandboxRoot = newRoot;
                return nullptr;
              }));

  env->define("sandbox",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>&) -> gecko::Value {
                return *sandboxRoot;
              }));

  // ── Utility ──

  env->define("glob",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("fs.glob(pattern) expects a string pattern");
                }
                auto pattern = args[0].as<std::string>();
                auto dir = fs::path(*sandboxRoot);
                auto arr = std::make_shared<std::vector<gecko::Value>>();
                for (const auto& entry : fs::recursive_directory_iterator(dir)) {
                  auto rel = fs::relative(entry.path(), dir).string();
                  // Simple wildcard match: check if rel contains the pattern
                  if (rel.find(pattern) != std::string::npos) {
                    arr->push_back(rel);
                  }
                }
                return arr;
              }));

  env->define("walk",
              gecko::NativeFunction([sandboxRoot](const std::vector<gecko::Value>& args) -> gecko::Value {
                std::string dir = args.empty() || !args[0].isString() ? "." : args[0].as<std::string>();
                auto path = resolvePath(*sandboxRoot, dir);
                auto sandbox = *sandboxRoot;
                auto arr = std::make_shared<std::vector<gecko::Value>>();
                for (const auto& entry : fs::recursive_directory_iterator(path)) {
                  arr->push_back(fs::relative(entry.path(), sandbox).string());
                }
                return arr;
              }));

  // ── Export Names ──

  env->exportName("read");
  env->exportName("readBytes");
  env->exportName("readLines");
  env->exportName("readJson");
  env->exportName("write");
  env->exportName("writeBytes");
  env->exportName("append");
  env->exportName("writeJson");
  env->exportName("mkdir");
  env->exportName("mkdirp");
  env->exportName("rmdir");
  env->exportName("rmrf");
  env->exportName("ls");
  env->exportName("lsFiles");
  env->exportName("lsDirs");
  env->exportName("tmpdir");
  env->exportName("cwd");
  env->exportName("exists");
  env->exportName("isFile");
  env->exportName("isDir");
  env->exportName("isSymlink");
  env->exportName("size");
  env->exportName("mtime");
  env->exportName("extension");
  env->exportName("basename");
  env->exportName("dirname");
  env->exportName("abs");
  env->exportName("copy");
  env->exportName("move");
  env->exportName("remove");
  env->exportName("link");
  env->exportName("symlink");
  env->exportName("chmod");
  env->exportName("stat");
  env->exportName("join");
  env->exportName("normalize");
  env->exportName("resolve");
  env->exportName("relative");
  env->exportName("sep");
  env->exportName("delimiter");
  env->exportName("chroot");
  env->exportName("sandbox");
  env->exportName("glob");
  env->exportName("walk");

  return env;
}

} // namespace module
