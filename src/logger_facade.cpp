#include "internal/logger_facade.h"
#include "internal/fs_util.h"
#include "internal/time_util.h"

#include "comm_thread.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace comm_log
{

static bool IsValidLevel(LogLevel level)
{
    return level >= LOG_LEVEL_TRACE && level <= LOG_LEVEL_NONE;
}

void* CALLBACK LoggerThread(void* arg)
{
    LoggerFacade* self = (LoggerFacade*)arg;
    self->Run();
    return NULL;
}

LoggerFacade& LoggerFacade::Instance()
{
    static LoggerFacade inst;
    return inst;
}

LoggerFacade::LoggerFacade()
    : m_running(false)
    , m_level(LOG_LEVEL_INFO)
    , m_console_level(LOG_LEVEL_INFO)
    , m_file_level(LOG_LEVEL_INFO)
    , m_thread(COMM_INVALID_THREAD)
    , m_last_flush_ms(0)
{
    memset(&m_config, 0, sizeof(m_config));
}

LoggerFacade::~LoggerFacade()
{
    Fini();
}

int32 LoggerFacade::Init(const CommLogConfig& config)
{
    std::lock_guard<std::mutex> guard(m_state_mutex);
    if (m_running.load())
    {
        return RET_OK;
    }

    m_config = config;
    if (!m_config.log_dir || m_config.log_dir[0] == 0)
    {
        return RET_ERROR;
    }
    LogLevel base_level = LOG_LEVEL_INFO;
    if (IsValidLevel(m_config.level))
    {
        base_level = m_config.level;
    }
    m_level.store(base_level);

    LogLevel console_level = base_level;
    if (IsValidLevel(m_config.console_level))
    {
        console_level = m_config.console_level;
    }
    m_console_level.store(console_level);

    LogLevel file_level = base_level;
    if (IsValidLevel(m_config.file_level))
    {
        file_level = m_config.file_level;
    }
    m_file_level.store(file_level);

    m_log_dir = m_config.log_dir;
    if (!FsUtil::EnsureDir(m_log_dir, 0777))
    {
        m_file_level.store(LOG_LEVEL_NONE);
        m_running.store(true);
    }
    m_retention.Configure(m_config.max_total_bytes, m_config.keep_days);

    uint64 now = TimeUtil::NowMs();
    TimeInfo t = TimeUtil::ToLocalTime(now);
    std::string date_key = TimeUtil::DateKey(t);
    m_retention.SetCurrentDateKey(date_key);
    m_last_flush_ms = now;

    m_dispatcher.Start(m_config.queue_capacity);
    m_running.store(true);

    m_thread = Thread::Create(LoggerThread, this, 0, false, 0, 0);
    if (m_thread == COMM_INVALID_THREAD)
    {
        m_running.store(false);
        return RET_ERROR;
    }

    TryCleanup(date_key);
    return RET_OK;
}

void LoggerFacade::Fini()
{
    {
        std::lock_guard<std::mutex> guard(m_state_mutex);
        if (!m_running.load())
        {
            return;
        }
        m_running.store(false);
    }

    m_dispatcher.Stop();
    m_dispatcher.NotifyAll();

    if (m_thread != COMM_INVALID_THREAD)
    {
        Thread::Wait(m_thread);
        m_thread = COMM_INVALID_THREAD;
    }

    m_sink.Flush();
    m_sink.Close();
}

void LoggerFacade::SetLevel(LogLevel level)
{
    m_level.store(level);
    m_console_level.store(level);
    m_file_level.store(level);
}

LogLevel LoggerFacade::GetLevel() const
{
    return m_level.load();
}

void LoggerFacade::SetConsoleLevel(LogLevel level)
{
    m_console_level.store(level);
}

LogLevel LoggerFacade::GetConsoleLevel() const
{
    return m_console_level.load();
}

void LoggerFacade::SetFileLevel(LogLevel level)
{
    m_file_level.store(level);
}

LogLevel LoggerFacade::GetFileLevel() const
{
    return m_file_level.load();
}

void LoggerFacade::Write(LogLevel level,
                         const char* file,
                         const char* func,
                         int line,
                         const char* format,
                         va_list args)
{
    if (!m_running.load())
    {
        return;
    }
    if (level == LOG_LEVEL_NONE)
    {
        return;
    }

    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);

    LogRecord record;
    record.level = level;
    record.timestamp_ms = TimeUtil::NowMs();
    record.line = line;
    record.file = file ? file : "";
    record.func = func ? func : "";
    record.message = buffer;
    record.date_key = TimeUtil::DateKey(TimeUtil::ToLocalTime(record.timestamp_ms));

    LogLevel console_level = m_console_level.load();
    LogLevel file_level = m_file_level.load();
    if (console_level == LOG_LEVEL_NONE && file_level == LOG_LEVEL_NONE)
    {
        return;
    }

    if (level >= console_level)
    {
        WriteConsole(record);
    }
    if (level >= file_level)
    {
        EnqueueRecord(record);
    }
}

void LoggerFacade::FatalAndAbort(const char* file,
                                 const char* func,
                                 int line,
                                 const char* format,
                                 va_list args)
{
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);

    LogRecord record;
    record.level = LOG_LEVEL_FATAL;
    record.timestamp_ms = TimeUtil::NowMs();
    record.line = line;
    record.file = file ? file : "";
    record.func = func ? func : "";
    record.message = buffer;
    record.date_key = TimeUtil::DateKey(TimeUtil::ToLocalTime(record.timestamp_ms));

    if (LOG_LEVEL_FATAL >= m_console_level.load())
    {
        WriteConsole(record);
    }

    {
        std::lock_guard<std::mutex> guard(m_sink_mutex);
        if (EnsureOpenForDate(record.date_key))
        {
            std::string line_text = m_formatter.Format(record);
            m_sink.WriteLine(line_text);
            m_sink.FlushSync();
        }
    }
    abort();
}

void LoggerFacade::Flush()
{
    std::lock_guard<std::mutex> guard(m_sink_mutex);
    m_sink.Flush();
}

void LoggerFacade::Run()
{
    while (m_running.load())
    {
        LogRecord record;
        if (!m_dispatcher.Pop(record))
        {
            continue;
        }

        {
            std::lock_guard<std::mutex> guard(m_sink_mutex);
            if (!EnsureOpenForDate(record.date_key))
            {
                continue;
            }
            std::string line = m_formatter.Format(record);
            m_sink.WriteLine(line);
            /*
             * 队列写空就落盘：日志稀疏时每条都及时进文件，不用等下一条日志来带，
             * 进程崩溃也不会丢掉最后几条。一直有日志进来时按 flush_interval_ms 攒一批再落。
             */
            uint64 now = TimeUtil::NowMs();
            if (m_dispatcher.Size() == 0 ||
                (m_config.flush_interval_ms > 0 && now - m_last_flush_ms >= m_config.flush_interval_ms))
            {
                m_sink.Flush();
                m_last_flush_ms = now;
            }
        }
    }

    LogRecord record;
    while (m_dispatcher.Pop(record))
    {
        {
            std::lock_guard<std::mutex> guard(m_sink_mutex);
            if (!EnsureOpenForDate(record.date_key))
            {
                continue;
            }
            std::string line = m_formatter.Format(record);
            m_sink.WriteLine(line);
        }
    }
}

bool LoggerFacade::EnsureOpenForDate(const std::string& date_key)
{
    if (m_sink.CurrentDateKey() == date_key && !m_sink.CurrentPath().empty())
    {
        return true;
    }
    if (!m_sink.Open(m_log_dir, date_key))
    {
        return false;
    }
    TryCleanup(date_key);
    return true;
}

void LoggerFacade::TryCleanup(const std::string& date_key)
{
    m_retention.SetCurrentDateKey(date_key);
    m_retention.Cleanup(m_log_dir);
}

void LoggerFacade::EnqueueRecord(const LogRecord& record)
{
    m_dispatcher.Push(record);
}

void LoggerFacade::WriteConsole(const LogRecord& record)
{
    const char* color = "\033[0m";
    const char* level_str = "INFO";
    switch (record.level)
    {
        case LOG_LEVEL_TRACE:
            color = "\033[35m";
            level_str = "TRAC";
            break;
        case LOG_LEVEL_DEBUG:
            color = "\033[32m";
            level_str = "DEBG";
            break;
        case LOG_LEVEL_INFO:
            color = "\033[34m";
            level_str = "INFO";
            break;
        case LOG_LEVEL_WARN:
            color = "\033[33m";
            level_str = "WARN";
            break;
        case LOG_LEVEL_ERROR:
            color = "\033[31m";
            level_str = "ERRO";
            break;
        case LOG_LEVEL_FATAL:
            color = "\033[31m";
            level_str = "FATL";
            break;
        default:
            break;
    }

    TimeInfo t = TimeUtil::ToLocalTime(record.timestamp_ms);
    std::string time_str = TimeUtil::TimeString(t);

    FILE* stream = (record.level >= LOG_LEVEL_ERROR) ? stderr : stdout;
    std::lock_guard<std::mutex> guard(m_console_mutex);
    fprintf(stream, "%s[%s][%s][%s:%d] %s\033[0m\n",
            color,
            time_str.c_str(),
            level_str,
            record.file.c_str(),
            record.line,
            record.message.c_str());
    fflush(stream);
}

}
