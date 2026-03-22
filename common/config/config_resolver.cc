#include "config_resolver.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace config
{

void ConfigResolver::resolve(const std::string &filename, const std::string &sniper_root,
                             std::vector<std::string> &resolved)
{
   if (std::find(resolved.begin(), resolved.end(), filename) != resolved.end())
      return;

   std::string path = filename;
   // If it's a simple name and sniper_root is provided, look in config/
   if (!sniper_root.empty() && path.find("/") == std::string::npos) {
      path = sniper_root + "/config/" + path;
   }
   // Append .cfg if missing
   if (path.find(".cfg") == std::string::npos) {
      path += ".cfg";
   }

   std::ifstream file(path);
   if (!file.is_open()) {
      // Fallback: try relative to current dir if not found in root/config
      if (path != filename) {
         std::ifstream file2(filename);
         if (file2.is_open()) {
            path = filename;
            file.swap(file2);
         }
         else {
            return;
         }
      }
      else {
         return;
      }
   }

   std::string line;
   while (std::getline(file, line)) {
      if (line.find("#include") == 0) {
         std::istringstream iss(line);
         std::string inc, inc_file;
         iss >> inc >> inc_file;
         if (!inc_file.empty()) {
            resolve(inc_file, sniper_root, resolved);
         }
      }
   }
   resolved.push_back(path);
}

} // namespace config
