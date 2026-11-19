#include "internal/formatter.h"
#include "internal/time_util.h"

#include <stdio.h>

namespace comm_log
{

std::string TextFormatter::Format(const LogRecord& record)
{
    TimeInfo t = TimeUtil::ToLocalTime(record.timestamp_ms);
    std::string time_str = TimeUtil::TimeString(t);

    const char* level_str = "INFO";
    switch (record.level)
    {
        case LOG_LEVEL_TRACE: level_str = "TRAC"; break;
        case LOG_LEVEL_DEBUG: level_str = "DEBG"; break;
        case LOG_LEVEL_INFO:  level_str = "INFO"; break;
        case LOG_LEVEL_WARN:  level_str = "WARN"; break;
        case LOG_LEVEL_ERROR: level_str = "ERRO"; break;
        case LOG_LEVEL_FATAL: level_str = "FATL"; break;
        default: break;
    }

    std::string out;
    out.reserve(record.message.size() + 128);

    char head[256];
    snprintf(head, sizeof(head), "[%s][%s][%s:%d] ",
             time_str.c_str(),
             level_str,
             record.file.c_str(),
             record.line);
    out.append(head);
    out.append(record.message);
    return out;
}

}
