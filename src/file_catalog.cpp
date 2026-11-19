#include "internal/file_catalog.h"
#include "internal/fs_util.h"

#include <algorithm>
#include <stdio.h>

namespace comm_log
{

static bool IsLogFileName(const std::string& name)
{
    if (name.size() != 14)
    {
        return false;
    }
    if (name[4] != '-' || name[7] != '-')
    {
        return false;
    }
    if (name.substr(10) != ".log")
    {
        return false;
    }
    for (size_t i = 0; i < 10; ++i)
    {
        if (i == 4 || i == 7)
        {
            continue;
        }
        if (name[i] < '0' || name[i] > '9')
        {
            return false;
        }
    }
    return true;
}

static bool IsLeapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int DaysInMonth(int year, int month)
{
    static const int kDays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12)
    {
        return 0;
    }
    if (month == 2 && IsLeapYear(year))
    {
        return 29;
    }
    return kDays[month];
}

static bool IsCalendarDate(int year, int month, int day)
{
    if (month < 1 || month > 12)
    {
        return false;
    }
    int days = DaysInMonth(year, month);
    return day >= 1 && day <= days;
}

bool FileCatalog::ParseDateKey(const std::string& name, std::string& date_key)
{
    if (!IsLogFileName(name))
    {
        return false;
    }
    int year = 0;
    int month = 0;
    int day = 0;
    if (sscanf(name.c_str(), "%4d-%2d-%2d", &year, &month, &day) != 3)
    {
        return false;
    }
    if (!IsCalendarDate(year, month, day))
    {
        return false;
    }
    date_key = name.substr(0, 10);
    return true;
}

bool FileCatalog::Scan(const std::string& dir)
{
    m_files.clear();
    m_total_size = 0;

    std::vector<std::string> names;
    if (!FsUtil::ListFiles(dir, names))
    {
        return false;
    }

    for (size_t i = 0; i < names.size(); ++i)
    {
        std::string date_key;
        if (!ParseDateKey(names[i], date_key))
        {
            continue;
        }

        LogFileInfo info;
        info.path = dir + "/" + names[i];
        info.date_key = date_key;
        FileStat st;
        if (!FsUtil::StatFile(info.path, st))
        {
            continue;
        }
        info.size = st.size;
        m_files.push_back(info);
        m_total_size += info.size;
    }

    std::sort(m_files.begin(), m_files.end(),
              [](const LogFileInfo& a, const LogFileInfo& b)
              {
                  return a.date_key < b.date_key;
              });

    return true;
}

uint64 FileCatalog::TotalSize() const
{
    return m_total_size;
}

const std::vector<LogFileInfo>& FileCatalog::Files() const
{
    return m_files;
}

bool FileCatalog::Oldest(LogFileInfo& info) const
{
    if (m_files.empty())
    {
        return false;
    }
    info = m_files.front();
    return true;
}

bool FileCatalog::Drop(const std::string& path)
{
    for (size_t i = 0; i < m_files.size(); ++i)
    {
        if (m_files[i].path != path)
        {
            continue;
        }
        if (m_total_size >= m_files[i].size)
        {
            m_total_size -= m_files[i].size;
        }
        else
        {
            m_total_size = 0;
        }
        m_files.erase(m_files.begin() + (std::vector<LogFileInfo>::difference_type)i);
        return true;
    }
    return false;
}

}
