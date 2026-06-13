#include "Object.h"
#include <format>
#include <iostream>

ObjString* copyString(const std::string& s)
{
  return new ObjString(s);
}

void printObject(Value value)
{
  std::cout << value.toString();
}
