#ifndef COMM_LOG_FILE_CATALOG_H
#define COMM_LOG_FILE_CATALOG_H

#include "comm_types.h"

#include <string>
#include <vector>

namespace comm_log
{

struct LogFileInfo
{
    std::string path;
    std::string date_key;
    uint64 size;
};

class FileCatalog
{
public:
    bool Scan(const std::string& dir);
    uint64 TotalSize() const;
    const std::vector<LogFileInfo>& Files() const;
    bool Oldest(LogFileInfo& info) const;
    bool Drop(const std::string& path);

    static bool ParseDateKey(const std::string& name, std::string& date_key);
    static bool PartNumber(const std::string& name, uint32& part);

private:
    std::vector<LogFileInfo> m_files;
    uint64 m_total_size;
};

}

#endif
