#include "internal/retention_policy.h"
#include "internal/fs_util.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

namespace comm_log
{

RetentionPolicy::RetentionPolicy()
    : m_max_total_bytes(0)
    , m_keep_days(0)
{
}

void RetentionPolicy::Configure(uint64 max_total_bytes, uint32 keep_days)
{
    m_max_total_bytes = max_total_bytes;
    m_keep_days = keep_days;
}

void RetentionPolicy::SetCurrentDateKey(const std::string& date_key)
{
    m_current_date_key = date_key;
}

bool RetentionPolicy::Cleanup(const std::string& dir)
{
    FileCatalog catalog;
    if (!catalog.Scan(dir))
    {
        return false;
    }

    bool changed = false;
    while (CleanupBySize(catalog))
    {
        changed = true;
        if (!catalog.Scan(dir))
        {
            return changed;
        }
    }
    while (CleanupByDays(catalog))
    {
        changed = true;
        if (!catalog.Scan(dir))
        {
            return changed;
        }
    }
    return changed;
}

bool RetentionPolicy::CleanupBySize(FileCatalog& catalog)
{
    if (m_max_total_bytes == 0)
    {
        return false;
    }
    if (catalog.TotalSize() <= m_max_total_bytes)
    {
        return false;
    }
    return RemoveOldest(catalog);
}

static int32 DaysDiff(const std::string& oldest, const std::string& current)
{
    int y1, m1, d1;
    int y2, m2, d2;
    if (sscanf(oldest.c_str(), "%4d-%2d-%2d", &y1, &m1, &d1) != 3)
    {
        return 0;
    }
    if (sscanf(current.c_str(), "%4d-%2d-%2d", &y2, &m2, &d2) != 3)
    {
        return 0;
    }
    struct tm t1;
    struct tm t2;
    memset(&t1, 0, sizeof(t1));
    memset(&t2, 0, sizeof(t2));
    t1.tm_year = y1 - 1900;
    t1.tm_mon = m1 - 1;
    t1.tm_mday = d1;
    t2.tm_year = y2 - 1900;
    t2.tm_mon = m2 - 1;
    t2.tm_mday = d2;
    time_t s1 = mktime(&t1);
    time_t s2 = mktime(&t2);
    if (s1 == (time_t)-1 || s2 == (time_t)-1)
    {
        return 0;
    }
    double diff = difftime(s2, s1);
    return (int32)(diff / 86400);
}

bool RetentionPolicy::CleanupByDays(FileCatalog& catalog)
{
    if (m_keep_days == 0)
    {
        return false;
    }
    if (m_current_date_key.empty())
    {
        return false;
    }

    LogFileInfo oldest;
    if (!catalog.Oldest(oldest))
    {
        return false;
    }
    if (oldest.date_key == m_current_date_key)
    {
        return false;
    }

    int32 diff = DaysDiff(oldest.date_key, m_current_date_key);
    if (diff < 0)
    {
        return false;
    }
    if ((uint32)diff < m_keep_days)
    {
        return false;
    }
    return RemoveOldest(catalog);
}

bool RetentionPolicy::RemoveOldest(FileCatalog& catalog)
{
    LogFileInfo oldest;
    if (!catalog.Oldest(oldest))
    {
        return false;
    }
    if (oldest.date_key == m_current_date_key)
    {
        return false;
    }
    return FsUtil::RemoveFile(oldest.path);
}

}
