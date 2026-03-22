#ifndef CONFIG_RESOLVER_H
#define CONFIG_RESOLVER_H

#include <string>
#include <vector>

namespace config
{

class ConfigResolver
{
 public:
   static void resolve(const std::string &filename, const std::string &sniper_root, std::vector<std::string> &resolved);
};

} // namespace config

#endif
