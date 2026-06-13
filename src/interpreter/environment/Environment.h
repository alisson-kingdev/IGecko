#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../runtime/Value.h"

/**
 * @brief Manages scope-based variable binding and resolution.
 */
class Environment
{
public:
  /** @brief Constructs an environment optionally linked to a parent (outer) scope. */
  explicit Environment(std::shared_ptr<Environment> parent = nullptr);

  /** @brief Defines a new variable in the current scope. */
  void define(const std::string& name, const gecko::Value& value, bool isConst = false);

  /** @brief Assigns a new value to an existing variable in the scope chain. */
  void assign(const std::string& name, const gecko::Value& value);

  /** @brief Retrieves the value of a variable from the scope chain. */
  gecko::Value get(const std::string& name) const;

  /** @brief Returns all names defined in the current scope. */
  std::vector<std::string> getDeclaredNames() const;

  /** @brief Returns all names that are exported from this environment. */
  std::vector<std::string> getExportedNames() const;

  /** @brief Checks if a name is exported in this environment. */
  bool isExported(const std::string& name) const;

  /** @brief Marks a variable name for export. */
  void exportName(const std::string& name);

  /** @brief Sets the package name associated with this environment. */
  void setPackageName(const std::string& name)
  {
    packageName = name;
  }

  /** @brief Returns the package name. */
  std::string getPackageName() const
  {
    return packageName;
  }

private:
  std::shared_ptr<Environment> parent;
  std::map<std::string, gecko::Value> values;
  std::map<std::string, bool> constants;
  std::vector<std::string> exports;
  std::string packageName;
};

#endif // ENVIRONMENT_H
