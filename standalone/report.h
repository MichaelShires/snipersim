#ifndef REPORT_GENERATOR_H
#define REPORT_GENERATOR_H

#include <string>
#include <vector>
#include <map>
#include <sqlite3.h>

class ReportGenerator {
public:
    ReportGenerator(const std::string& db_path);
    ~ReportGenerator();

    bool generate(const std::string& output_path);

private:
    sqlite3* m_db;
    std::string m_db_path;

    struct Metric {
        int id;
        std::string object;
        std::string name;
    };

    std::map<int, Metric> m_metrics;
    std::map<std::string, int> m_prefixes;

    bool loadMetadata();
    std::map<int, std::map<int, double>> getValues(int prefix_id);
};

#endif // REPORT_GENERATOR_H
