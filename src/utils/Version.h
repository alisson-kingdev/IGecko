#ifndef GECKO_VERSION_H
#define GECKO_VERSION_H

#include <string>

namespace gecko
{

inline std::string getVersion()
{
  return "0.1.0";
}

inline std::string getProjectName()
{
  return "IGecko";
}

inline std::string getInterpreterName()
{
  return "Gecko VM";
}

} // namespace gecko

#endif // GECKO_VERSION_H
