#include "internal/retention_policy.h"
#include "internal/fs_util.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <vector>

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

bool RetentionPolicy::Cleanup(const std::string& dir, const std::string& active_path)
{
    FileCatalog catalog;
    if (!catalog.Scan(dir))
    {
        return false;
    }
    if (!CleanupByDays(catalog))
    {
        return false;
    }
    return CleanupBySize(catalog, active_path);
}

static bool IsSkipped(const std::vector<std::string>& skipped, const std::string& path)
{
    for (size_t i = 0; i < skipped.size(); ++i)
    {
        if (skipped[i] == path)
        {
            return true;
        }
    }
    return false;
}

static bool IsLeapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static bool IsCalendarDate(int year, int month, int day)
{
    static const int kDays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12 || day < 1)
    {
        return false;
    }
    int days = kDays[month];
    if (month == 2 && IsLeapYear(year))
    {
        days = 29;
    }
    return day <= days;
}

static bool DaysDiff(const std::string& oldest, const std::string& current, int32* days)
{
    int y1 = 0;
    int m1 = 0;
    int d1 = 0;
    int y2 = 0;
    int m2 = 0;
    int d2 = 0;
    if (sscanf(oldest.c_str(), "%4d-%2d-%2d", &y1, &m1, &d1) != 3)
    {
        return false;
    }
    if (sscanf(current.c_str(), "%4d-%2d-%2d", &y2, &m2, &d2) != 3)
    {
        return false;
    }
    if (!IsCalendarDate(y1, m1, d1) || !IsCalendarDate(y2, m2, d2))
    {
        return false;
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
    t1.tm_isdst = -1;
    t2.tm_isdst = -1;
    time_t s1 = mktime(&t1);
    time_t s2 = mktime(&t2);
    if (s1 == (time_t)-1 || s2 == (time_t)-1)
    {
        return false;
    }
    *days = (int32)(difftime(s2, s1) / 86400);
    return true;
}

bool RetentionPolicy::CleanupBySize(FileCatalog& catalog, const std::string& active_path)
{
    if (m_max_total_bytes == 0)
    {
        return true;
    }
    std::string protected_path = active_path;
    if (protected_path.empty() && !catalog.Files().empty() &&
        catalog.Files().back().date_key == m_current_date_key)
    {
        protected_path = catalog.Files().back().path;
    }

    std::vector<std::string> skipped;
    while (catalog.TotalSize() > m_max_total_bytes)
    {
        bool found = false;
        LogFileInfo target;
        const std::vector<LogFileInfo>& files = catalog.Files();
        for (size_t i = 0; i < files.size(); ++i)
        {
            if (files[i].path == protected_path)
            {
                continue;
            }
            if (IsSkipped(skipped, files[i].path))
            {
                continue;
            }
            target = files[i];
            found = true;
            break;
        }
        if (!found)
        {
            return false;
        }
        if (!FsUtil::RemoveFile(target.path))
        {
            skipped.push_back(target.path);
            continue;
        }
        catalog.Drop(target.path);
    }
    return true;
}

bool RetentionPolicy::CleanupByDays(FileCatalog& catalog)
{
    if (m_keep_days == 0 || m_current_date_key.empty())
    {
        return true;
    }

    std::vector<std::string> skipped;
    for (;;)
    {
        bool found = false;
        LogFileInfo target;
        const std::vector<LogFileInfo>& files = catalog.Files();
        for (size_t i = 0; i < files.size(); ++i)
        {
            if (IsSkipped(skipped, files[i].path))
            {
                continue;
            }
            if (files[i].date_key == m_current_date_key)
            {
                return true;
            }
            int32 diff = 0;
            if (!DaysDiff(files[i].date_key, m_current_date_key, &diff))
            {
                skipped.push_back(files[i].path);
                continue;
            }
            if (diff < 0 || (uint32)diff < m_keep_days)
            {
                return true;
            }
            target = files[i];
            found = true;
            break;
        }
        if (!found)
        {
            return true;
        }
        if (!FsUtil::RemoveFile(target.path))
        {
            skipped.push_back(target.path);
            continue;
        }
        catalog.Drop(target.path);
    }
}

bool RetentionPolicy::RemoveOldest(FileCatalog& catalog, const std::string& active_path)
{
    LogFileInfo oldest;
    if (!catalog.Oldest(oldest))
    {
        return false;
    }
    if (oldest.path == active_path)
    {
        return false;
    }
    if (!FsUtil::RemoveFile(oldest.path))
    {
        return false;
    }
    catalog.Drop(oldest.path);
    return true;
}

}
