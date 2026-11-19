#ifndef COMM_LOG_TIME_UTIL_H
#define COMM_LOG_TIME_UTIL_H

#include "comm_types.h"
#include <string>

namespace comm_log
{

struct TimeInfo
{
    uint32 year;
    uint32 month;
    uint32 day;
    uint32 hour;
    uint32 minute;
    uint32 second;
    uint32 millisecond;
};

class TimeUtil
{
public:
    static uint64 NowMs();
    static TimeInfo ToLocalTime(uint64 timestamp_ms);
    static std::string DateKey(const TimeInfo& t);
    static std::string TimeString(const TimeInfo& t);
};

}

#endif
