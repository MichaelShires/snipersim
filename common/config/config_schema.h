#ifndef CONFIG_SCHEMA_H
#define CONFIG_SCHEMA_H

#include "fixed_types.h"
#include "config.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>

namespace config {

class ConfigSchema {
public:
    enum ValueType {
        V_BOOL,
        V_INT,
        V_FLOAT,
        V_STRING
    };

    struct SchemaEntry {
        std::string path;
        ValueType type;
        bool required;
        std::string description;
        // Optional range or allowed values could be added here
    };

    ConfigSchema();
    void addEntry(const std::string& path, ValueType type, bool required = false, const std::string& description = "");
    
    // Validates the provided config against this schema
    // Returns true if valid, throws SniperException otherwise
    bool validate(Config& config) const;

    // Static method to get the global schema for Sniper
    static ConfigSchema& getGlobal();

private:
    std::map<std::string, SchemaEntry> m_entries;
    std::set<std::string> m_allowed_prefixes; // For wildcard sections if needed

    void walkConfig(const Section& section, const std::string& currentPath, std::set<std::string>& foundPaths) const;
};

} // namespace config

#endif // CONFIG_SCHEMA_H
