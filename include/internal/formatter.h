#ifndef COMM_LOG_FORMATTER_H
#define COMM_LOG_FORMATTER_H

#include "internal/log_record.h"

#include <string>

namespace comm_log
{

class Formatter
{
public:
    virtual ~Formatter()
    {
    }

    virtual std::string Format(const LogRecord& record) = 0;
};

class TextFormatter : public Formatter
{
public:
    virtual std::string Format(const LogRecord& record);
};

}

#endif
