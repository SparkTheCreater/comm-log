#include "comm_log_api.h"
#include "internal/logger_facade.h"

#include <stdarg.h>

using namespace comm_log;

int32 comm_log_init(const CommLogConfig* config)
{
    if (!config)
    {
        return RET_ERROR;
    }
    return LoggerFacade::Instance().Init(*config);
}

void comm_log_fini(void)
{
    LoggerFacade::Instance().Fini();
}

void comm_log_set_level(LogLevel level)
{
    LoggerFacade::Instance().SetLevel(level);
}

LogLevel comm_log_get_level(void)
{
    return LoggerFacade::Instance().GetLevel();
}

void comm_log_set_console_level(LogLevel level)
{
    LoggerFacade::Instance().SetConsoleLevel(level);
}

LogLevel comm_log_get_console_level(void)
{
    return LoggerFacade::Instance().GetConsoleLevel();
}

void comm_log_set_file_level(LogLevel level)
{
    LoggerFacade::Instance().SetFileLevel(level);
}

LogLevel comm_log_get_file_level(void)
{
    return LoggerFacade::Instance().GetFileLevel();
}

void comm_log_flush(void)
{
    LoggerFacade::Instance().Flush();
}

int32 comm_log_get_last_io_error(void)
{
    return LoggerFacade::Instance().LastIoError();
}

void comm_log_write(LogLevel level,
                    const char* file,
                    const char* func,
                    int line,
                    const char* format,
                    ...)
{
    va_list args;
    va_start(args, format);
    comm_log_write_v(level, file, func, line, format, args);
    va_end(args);
}

void comm_log_write_v(LogLevel level,
                      const char* file,
                      const char* func,
                      int line,
                      const char* format,
                      va_list args)
{
    LoggerFacade::Instance().Write(level, file, func, line, format, args);
}

void comm_log_fatal(const char* file,
                    const char* func,
                    int line,
                    const char* format,
                    ...)
{
    va_list args;
    va_start(args, format);
    comm_log_fatal_v(file, func, line, format, args);
    va_end(args);
}

void comm_log_fatal_v(const char* file,
                      const char* func,
                      int line,
                      const char* format,
                      va_list args)
{
    LoggerFacade::Instance().FatalAndAbort(file, func, line, format, args);
}
