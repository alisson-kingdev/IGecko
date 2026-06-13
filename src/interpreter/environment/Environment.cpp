#include "Environment.h"

#include <algorithm>
#include <stdexcept>

Environment::Environment(std::shared_ptr<Environment> parent_) : parent(std::move(parent_)) {}

void Environment::define(const std::string& name, const gecko::Value& value, bool isConst)
{
  values[name] = value;
  constants[name] = isConst;
}

void Environment::assign(const std::string& name, const gecko::Value& value)
{
  auto it = values.find(name);
  if (it != values.end()) {
    auto constIt = constants.find(name);
    if (constIt != constants.end() && constIt->second) {
      throw std::runtime_error("Cannot assign to constant '" + name + "'");
    }
    it->second = value;
    return;
  }
  if (parent) {
    parent->assign(name, value);
    return;
  }
  throw std::runtime_error("Undefined variable '" + name + "'");
}

gecko::Value Environment::get(const std::string& name) const
{
  auto it = values.find(name);
  if (it != values.end()) {
    return it->second;
  }
  if (parent) {
    return parent->get(name);
  }
  throw std::runtime_error("Undefined variable '" + name + "'");
}

std::vector<std::string> Environment::getDeclaredNames() const
{
  std::vector<std::string> names;
  names.reserve(values.size());
  for (const auto& entry : values) {
    names.push_back(entry.first);
  }
  return names;
}

void Environment::exportName(const std::string& name)
{
  if (std::find(exports.begin(), exports.end(), name) == exports.end()) {
    exports.push_back(name);
  }
}

std::vector<std::string> Environment::getExportedNames() const
{
  if (!exports.empty()) {
    return exports;
  }
  return getDeclaredNames();
}

bool Environment::isExported(const std::string& name) const
{
  if (exports.empty()) {
    return true;
  }
  return std::find(exports.begin(), exports.end(), name) != exports.end();
}
