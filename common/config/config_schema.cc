#include "config_schema.h"
#include "sniper_exception.h"
#include <iostream>
#include <sstream>

namespace config {

ConfigSchema::ConfigSchema() {
    // Basic core schema
    addEntry("general/arch", V_STRING, true, "Target architecture (intel, riscv)");
    addEntry("general/mode", V_STRING, true, "Bit mode (32, 64)");
    addEntry("general/total_cores", V_INT, true, "Total number of simulated cores");
    addEntry("general/output_dir", V_STRING, false, "Output directory for results");
    
    addEntry("traceinput/enabled", V_BOOL, false, "Enable trace-driven simulation");
    addEntry("traceinput/trace_prefix", V_STRING, false, "Prefix for trace files");
    addEntry("traceinput/num_runs", V_INT, false, "Number of simulation runs");
    
    addEntry("perf_model/core/type", V_STRING, false, "Core performance model type");
    addEntry("perf_model/l1_icache/enabled", V_BOOL, false);
    addEntry("perf_model/l1_dcache/enabled", V_BOOL, false);
    
    // Wildcard sections (prefixes that are allowed to have any keys)
    m_allowed_prefixes.insert("hooks/");
    m_allowed_prefixes.insert("network/");
    m_allowed_prefixes.insert("perf_model/");
    m_allowed_prefixes.insert("traceinput/");
    m_allowed_prefixes.insert("scheduler/");
    m_allowed_prefixes.insert("caching_protocol/");
    m_allowed_prefixes.insert("power/");
    m_allowed_prefixes.insert("dvfs/");
    m_allowed_prefixes.insert("clock_skew_minimization/");
    m_allowed_prefixes.insert("sampling/");
    m_allowed_prefixes.insert("roi/");
    m_allowed_prefixes.insert("log/");
    m_allowed_prefixes.insert("fault_injection/");
    m_allowed_prefixes.insert("instruction_tracer/");
    m_allowed_prefixes.insert("queue_model/");
    m_allowed_prefixes.insert("barrier/");
    m_allowed_prefixes.insert("vcores/");
    m_allowed_prefixes.insert("tags/");
    m_allowed_prefixes.insert("memory_tracker/");
    m_allowed_prefixes.insert("routine_tracer/");
    m_allowed_prefixes.insert("thread_stats_manager/");
    m_allowed_prefixes.insert("bbv/");
    m_allowed_prefixes.insert("core/");
    m_allowed_prefixes.insert("general/");
    m_allowed_prefixes.insert("loop_tracer/");
    m_allowed_prefixes.insert("osemu/");
    m_allowed_prefixes.insert("progress_trace/");
}

void ConfigSchema::addEntry(const std::string& path, ValueType type, bool required, const std::string& description) {
    m_entries[path] = {path, type, required, description};
}

bool ConfigSchema::validate(Config& config) const {
    std::set<std::string> foundPaths;
    walkConfig(config.getRoot(), "", foundPaths);

    bool all_ok = true;
    std::stringstream errors;

    // 1. Check for missing required keys
    for (auto const& [path, entry] : m_entries) {
        if (entry.required && foundPaths.find(path) == foundPaths.end()) {
            errors << "Missing required configuration key: " << path << "\n";
            all_ok = false;
        }
    }

    // 2. Check for unknown keys (potential typos)
    for (auto const& path : foundPaths) {
        bool allowed = false;
        if (m_entries.count(path)) {
            allowed = true;
        } else {
            for (auto const& prefix : m_allowed_prefixes) {
                if (path.compare(0, prefix.length(), prefix) == 0) {
                    allowed = true;
                    break;
                }
            }
        }

        if (!allowed) {
            errors << "Unknown configuration key found (possible typo): " << path << "\n";
            all_ok = false;
        }
    }

    if (!all_ok) {
        throw ConfigurationException("Pre-flight validation failed:\n" + errors.str());
    }

    return true;
}

void ConfigSchema::walkConfig(const Section& section, const std::string& currentPath, std::set<std::string>& foundPaths) const {
    // Keys in this section
    for (auto const& [name, key] : section.getKeys()) {
        foundPaths.insert(currentPath + name);
    }
    
    // Array keys
    for (auto const& [name, keys] : section.getArrayKeys()) {
        foundPaths.insert(currentPath + name);
    }

    // Subsections
    for (auto const& [name, subSection] : section.getSubsections()) {
        walkConfig(*subSection, currentPath + name + "/", foundPaths);
    }
}

ConfigSchema& ConfigSchema::getGlobal() {
    static ConfigSchema globalSchema;
    return globalSchema;
}

} // namespace config
