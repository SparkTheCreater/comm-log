#include "internal/logger_facade.h"
#include "internal/fs_util.h"
#include "internal/time_util.h"

#include "comm_thread.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <vector>

namespace comm_log
{

static void (*g_fatal_abort)() = abort;

void SetFatalAbortHook(void (*fn)())
{
    g_fatal_abort = fn ? fn : abort;
}

static bool IsValidLevel(LogLevel level)
{
    return level >= LOG_LEVEL_TRACE && level <= LOG_LEVEL_NONE;
}

static bool FormatMessage(const char* format, va_list args, std::string& message)
{
    if (!format)
    {
        message.clear();
        return true;
    }
    va_list measure_args;
    va_copy(measure_args, args);
    int length = vsnprintf(NULL, 0, format, measure_args);
    va_end(measure_args);
    if (length < 0)
    {
        return false;
    }
    std::vector<char> buffer((size_t)length + 1);
    int written = vsnprintf(buffer.data(), buffer.size(), format, args);
    if (written != length)
    {
        return false;
    }
    message.assign(buffer.data(), (size_t)length);
    return true;
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
        return RET_ERROR;
    }
    if (!config.log_dir || config.log_dir[0] == 0)
    {
        return RET_ERROR;
    }
    if (!IsValidLevel(config.level) ||
        !IsValidLevel(config.console_level) ||
        !IsValidLevel(config.file_level) ||
        config.queue_capacity == 0)
    {
        return RET_ERROR;
    }

    CommLogConfig saved_config = m_config;
    std::string saved_dir = m_log_dir;
    LogLevel saved_level = m_level.load();
    LogLevel saved_console = m_console_level.load();
    LogLevel saved_file = m_file_level.load();
    uint64 saved_max_bytes = 0;
    uint32 saved_keep_days = 0;

    m_config = config;
    m_level.store(config.level);
    m_console_level.store(config.console_level);
    m_file_level.store(config.file_level);
    m_log_dir = config.log_dir;

    if (!FsUtil::EnsureDir(m_log_dir, 0777))
    {
        m_config = saved_config;
        m_log_dir = saved_dir;
        m_level.store(saved_level);
        m_console_level.store(saved_console);
        m_file_level.store(saved_file);
        m_running.store(false);
        return RET_ERROR;
    }

    m_retention.Configure(config.max_total_bytes, config.keep_days);

    uint64 now = TimeUtil::NowMs();
    TimeInfo t = TimeUtil::ToLocalTime(now);
    std::string date_key = TimeUtil::DateKey(t);
    m_retention.SetCurrentDateKey(date_key);
    m_last_flush_ms = now;

    m_dispatcher.Start(config.queue_capacity);
    m_running.store(true);

    m_thread = Thread::Create(LoggerThread, this, 0, false, 0, 0);
    if (m_thread == COMM_INVALID_THREAD)
    {
        m_running.store(false);
        m_dispatcher.Stop();
        m_dispatcher.NotifyAll();
        m_config = saved_config;
        m_log_dir = saved_dir;
        m_level.store(saved_level);
        m_console_level.store(saved_console);
        m_file_level.store(saved_file);
        m_retention.Configure(saved_max_bytes, saved_keep_days);
        return RET_ERROR;
    }

    if (!TryCleanup(date_key))
    {
        m_running.store(false);
        m_dispatcher.Stop();
        m_dispatcher.NotifyAll();
        if (m_thread != COMM_INVALID_THREAD)
        {
            Thread::Wait(m_thread);
            m_thread = COMM_INVALID_THREAD;
        }
        m_sink.Close();
        m_config = saved_config;
        m_log_dir = saved_dir;
        m_level.store(saved_level);
        m_console_level.store(saved_console);
        m_file_level.store(saved_file);
        m_retention.Configure(saved_max_bytes, saved_keep_days);
        return RET_ERROR;
    }
    return RET_OK;
}

void LoggerFacade::Fini()
{
    std::lock_guard<std::mutex> guard(m_state_mutex);
    if (!m_running.load())
    {
        return;
    }
    m_running.store(false);
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
    if (!format || !IsValidLevel(level) || level == LOG_LEVEL_NONE || level < m_level.load())
    {
        return;
    }

    std::string message;
    if (!FormatMessage(format, args, message))
    {
        return;
    }

    LogRecord record;
    record.level = level;
    record.timestamp_ms = TimeUtil::NowMs();
    record.line = line;
    record.file = file ? file : "";
    record.func = func ? func : "";
    record.message = message;
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
    std::string message;
    if (!FormatMessage(format, args, message))
    {
        message = "failed to format fatal log";
    }

    LogRecord record;
    record.level = LOG_LEVEL_FATAL;
    record.timestamp_ms = TimeUtil::NowMs();
    record.line = line;
    record.file = file ? file : "";
    record.func = func ? func : "";
    record.message = message;
    record.date_key = TimeUtil::DateKey(TimeUtil::ToLocalTime(record.timestamp_ms));

    if (LOG_LEVEL_FATAL >= m_console_level.load())
    {
        WriteConsole(record);
    }

    if (m_running.load() && !m_log_dir.empty())
    {
        std::lock_guard<std::mutex> guard(m_sink_mutex);
        if (EnsureOpenForDate(record.date_key))
        {
            std::string line_text = m_formatter.Format(record);
            m_sink.WriteLine(line_text);
            m_sink.FlushSync();
        }
    }
    g_fatal_abort();
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
    if (m_sink.IsOpen() && m_sink.CurrentDateKey() == date_key)
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

bool LoggerFacade::TryCleanup(const std::string& date_key)
{
    m_retention.SetCurrentDateKey(date_key);
    return m_retention.Cleanup(m_log_dir);
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
