#include "internal/time_util.h"

#include <stdio.h>
#include <sys/time.h>
#include <time.h>

namespace comm_log
{

uint64 TimeUtil::NowMs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64)tv.tv_sec * 1000 + (uint64)(tv.tv_usec / 1000);
}

TimeInfo TimeUtil::ToLocalTime(uint64 timestamp_ms)
{
    time_t sec = (time_t)(timestamp_ms / 1000);
    struct tm tmval;
    localtime_r(&sec, &tmval);

    TimeInfo info;
    info.year = (uint32)(tmval.tm_year + 1900);
    info.month = (uint32)(tmval.tm_mon + 1);
    info.day = (uint32)tmval.tm_mday;
    info.hour = (uint32)tmval.tm_hour;
    info.minute = (uint32)tmval.tm_min;
    info.second = (uint32)tmval.tm_sec;
    info.millisecond = (uint32)(timestamp_ms % 1000);
    return info;
}

std::string TimeUtil::DateKey(const TimeInfo& t)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%04u-%02u-%02u", t.year, t.month, t.day);
    return std::string(buf);
}

std::string TimeUtil::TimeString(const TimeInfo& t)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
             t.year, t.month, t.day, t.hour, t.minute, t.second);
    return std::string(buf);
}

}
