#ifndef COMM_LOG_FACADE_H
#define COMM_LOG_FACADE_H

#include "comm_log_api.h"
#include "comm_thread.h"
#include "internal/async_dispatcher.h"
#include "internal/file_sink.h"
#include "internal/formatter.h"
#include "internal/retention_policy.h"

#include <atomic>
#include <mutex>
#include <string>

namespace comm_log
{

void* CALLBACK LoggerThread(void* arg);

class LoggerFacade
{
public:
    static LoggerFacade& Instance();

    int32 Init(const CommLogConfig& config);
    void Fini();

    void SetLevel(LogLevel level);
    LogLevel GetLevel() const;
    void SetConsoleLevel(LogLevel level);
    LogLevel GetConsoleLevel() const;
    void SetFileLevel(LogLevel level);
    LogLevel GetFileLevel() const;

    void Write(LogLevel level,
               const char* file,
               const char* func,
               int line,
               const char* format,
               va_list args);

    void Flush();
    void FatalAndAbort(const char* file,
                       const char* func,
                       int line,
                       const char* format,
                       va_list args);

private:
    LoggerFacade();
    ~LoggerFacade();
    LoggerFacade(const LoggerFacade&);
    LoggerFacade& operator=(const LoggerFacade&);

    void Run();
    bool EnsureOpenForDate(const std::string& date_key);
    bool TryCleanup(const std::string& date_key);
    void EnqueueRecord(LogRecord& record);
    void WriteConsole(const LogRecord& record);
    friend void* CALLBACK LoggerThread(void* arg);

private:
    CommLogConfig m_config;
    std::string m_log_dir;
    std::atomic<bool> m_running;
    std::atomic<LogLevel> m_level;
    std::atomic<LogLevel> m_console_level;
    std::atomic<LogLevel> m_file_level;
    AsyncDispatcher m_dispatcher;
    FileSink m_sink;
    TextFormatter m_formatter;
    RetentionPolicy m_retention;
    COMM_THREAD_HANDLE m_thread;
    std::mutex m_state_mutex;
    std::mutex m_sink_mutex;
    std::mutex m_console_mutex;
    uint64 m_last_flush_ms;
};

}

#endif
