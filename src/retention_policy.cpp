#include "internal/retention_policy.h"
#include "internal/fs_util.h"

#include <stdint.h>
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

static int64 CalendarDay(int year, int month, int day)
{
    year -= month <= 2;
    int era = year >= 0 ? year / 400 : (year - 399) / 400;
    unsigned int year_of_era = (unsigned int)(year - era * 400);
    unsigned int day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    unsigned int day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    return (int64)era * 146097 + day_of_era;
}

static bool ParseDay(const std::string& date, int64& day)
{
    int year = 0;
    int month = 0;
    int month_day = 0;
    char extra = 0;
    if (sscanf(date.c_str(), "%4d-%2d-%2d%c", &year, &month, &month_day, &extra) != 3 ||
        !IsCalendarDate(year, month, month_day))
    {
        return false;
    }
    day = CalendarDay(year, month, month_day);
    return true;
}

static bool DaysDiff(const std::string& oldest, const std::string& current, int32* days)
{
    int64 first = 0;
    int64 last = 0;
    if (!ParseDay(oldest, first) || !ParseDay(current, last))
    {
        return false;
    }
    int64 difference = last - first;
    if (difference < INT32_MIN || difference > INT32_MAX)
    {
        return false;
    }
    *days = (int32)difference;
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
