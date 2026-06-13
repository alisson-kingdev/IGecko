#ifndef FS_MODULE_H
#define FS_MODULE_H

#include <memory>

#include "../environment/Environment.h"

namespace module
{
std::shared_ptr<Environment> createFsModule(std::shared_ptr<Environment> globals);
}

#endif
