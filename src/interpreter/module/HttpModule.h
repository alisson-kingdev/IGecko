#ifndef HTTP_MODULE_H
#define HTTP_MODULE_H

#include <memory>

#include "../environment/Environment.h"

namespace module
{
std::shared_ptr<Environment> createHttpModule(std::shared_ptr<Environment> globals);
}

#endif // HTTP_MODULE_H
