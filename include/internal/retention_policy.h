#ifndef COMM_LOG_RETENTION_POLICY_H
#define COMM_LOG_RETENTION_POLICY_H

#include "comm_types.h"
#include "internal/file_catalog.h"

#include <string>

namespace comm_log
{

class RetentionPolicy
{
public:
    RetentionPolicy();

    void Configure(uint64 max_total_bytes, uint32 keep_days);
    void SetCurrentDateKey(const std::string& date_key);
    bool Cleanup(const std::string& dir, const std::string& active_path = std::string());

private:
    bool CleanupBySize(FileCatalog& catalog, const std::string& active_path);
    bool CleanupByDays(FileCatalog& catalog);
    bool RemoveOldest(FileCatalog& catalog, const std::string& active_path);

private:
    uint64 m_max_total_bytes;
    uint32 m_keep_days;
    std::string m_current_date_key;
};

}

#endif
