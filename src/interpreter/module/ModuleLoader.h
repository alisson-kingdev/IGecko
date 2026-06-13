#ifndef MODULELOADER_H
#define MODULELOADER_H

#include <memory>
#include <string>

#include "../environment/Environment.h"
#include "../runtime/Runtime.h"

class ModuleLoader
{
public:
  ModuleLoader(std::shared_ptr<Environment> globals_,
               std::shared_ptr<ModuleCache> moduleCache_,
               const std::string& sourceFile_);
  std::shared_ptr<Environment> loadModule(const std::string& modulePath);

private:
  std::shared_ptr<Environment> globals;
  std::shared_ptr<ModuleCache> moduleCache;
  std::string sourceFile;
};

#endif // MODULELOADER_H
