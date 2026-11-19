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
    bool Cleanup(const std::string& dir);

private:
    bool CleanupBySize(FileCatalog& catalog);
    bool CleanupByDays(FileCatalog& catalog);
    bool RemoveOldest(FileCatalog& catalog);

private:
    uint64 m_max_total_bytes;
    uint32 m_keep_days;
    std::string m_current_date_key;
};

}

#endif
