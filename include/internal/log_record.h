#ifndef COMM_LOG_RECORD_H
#define COMM_LOG_RECORD_H

#include "comm_log_api.h"

#include <stdint.h>
#include <string>

namespace comm_log
{

struct LogRecord
{
    LogLevel level;
    uint64 timestamp_ms;
    int line;
    std::string file;
    std::string func;
    std::string message;
    std::string date_key;
};

}

#endif
