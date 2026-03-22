#include "report.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>

ReportGenerator::ReportGenerator(const std::string& db_path) : m_db(nullptr), m_db_path(db_path) {
    if (sqlite3_open(db_path.c_str(), &m_db) != SQLITE_OK) {
        std::cerr << "[REPORT] Failed to open stats database: " << db_path << std::endl;
        m_db = nullptr;
    }
}

ReportGenerator::~ReportGenerator() {
    if (m_db) sqlite3_close(m_db);
}

bool ReportGenerator::loadMetadata() {
    if (!m_db) return false;

    const char* sql_prefixes = "SELECT prefixid, prefixname FROM prefixes";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql_prefixes, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            const char* name = (const char*)sqlite3_column_text(stmt, 1);
            m_prefixes[name] = id;
        }
        sqlite3_finalize(stmt);
    }

    const char* sql_names = "SELECT nameid, objectname, metricname FROM names";
    if (sqlite3_prepare_v2(m_db, sql_names, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Metric m;
            m.id = sqlite3_column_int(stmt, 0);
            m.object = (const char*)sqlite3_column_text(stmt, 1);
            m.name = (const char*)sqlite3_column_text(stmt, 2);
            m_metrics[m.id] = m;
        }
        sqlite3_finalize(stmt);
    }

    return true;
}

std::map<int, std::map<int, double>> ReportGenerator::getValues(int prefix_id) {
    std::map<int, std::map<int, double>> values;
    const char* sql = "SELECT nameid, core, value FROM values WHERE prefixid = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, prefix_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int nameid = sqlite3_column_int(stmt, 0);
            int core = sqlite3_column_int(stmt, 1);
            double value = sqlite3_column_double(stmt, 2);
            values[nameid][core] = value;
        }
        sqlite3_finalize(stmt);
    }
    return values;
}

bool ReportGenerator::generate(const std::string& output_path) {
    if (!loadMetadata()) return false;

    int prefix_id = -1;
    if (m_prefixes.count("roi-end")) {
        prefix_id = m_prefixes["roi-end"];
    } else if (!m_prefixes.empty()) {
        prefix_id = m_prefixes.rbegin()->second; // last one
    } else {
        return false;
    }

    auto values = getValues(prefix_id);
    
    std::ofstream out(output_path);
    if (!out.is_open()) return false;

    out << "=== Sniper Simulation Summary (Native C++ Report) ===\n\n";

    // Find core metrics
    int instr_id = -1, time_id = -1;
    for (auto const& [id, m] : m_metrics) {
        if (m.object == "performance_model" && m.name == "instruction_count") instr_id = id;
        if (m.object == "performance_model" && m.name == "elapsed_time") time_id = id;
    }

    if (instr_id != -1 && time_id != -1) {
        auto const& instrs = values[instr_id];
        auto const& times = values[time_id];
        
        int ncores = instrs.size();
        out << std::left << std::setw(20) << "Core" << " | ";
        for (int i = 0; i < ncores; ++i) out << std::setw(15) << ("Core " + std::to_string(i));
        out << "\n" << std::string(20 + 3 + 15 * ncores, '-') << "\n";

        out << std::left << std::setw(20) << "Instructions" << " | ";
        for (int i = 0; i < ncores; ++i) out << std::setw(15) << (long)instrs.at(i);
        out << "\n";

        out << std::left << std::setw(20) << "Time (ns)" << " | ";
        for (int i = 0; i < ncores; ++i) out << std::setw(15) << (long)(times.at(i) / 1e6);
        out << "\n";

        out << std::left << std::setw(20) << "IPC" << " | ";
        for (int i = 0; i < ncores; ++i) {
            double ipc = 0;
            // Simplified cycle count calculation for this demo
            // In a real report, we'd fetch cycles too
            out << std::setw(15) << std::fixed << std::setprecision(2) << 1.0; // Placeholder
        }
        out << "\n";
    }

    out.close();
    std::cout << "[REPORT] Native report generated: " << output_path << std::endl;
    return true;
}
