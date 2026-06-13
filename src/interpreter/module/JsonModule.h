#ifndef JSON_MODULE_H
#define JSON_MODULE_H

#include <memory>

#include "../environment/Environment.h"

namespace module
{
std::shared_ptr<Environment> createJsonModule(std::shared_ptr<Environment> globals);
}

#endif // JSON_MODULE_H
