#ifndef COMM_LOG_ASYNC_DISPATCHER_H
#define COMM_LOG_ASYNC_DISPATCHER_H

#include "comm_log_api.h"
#include "internal/log_record.h"

#include <condition_variable>
#include <deque>
#include <mutex>

namespace comm_log
{

class AsyncDispatcher
{
public:
    AsyncDispatcher();
    ~AsyncDispatcher();

    bool Start(uint32 capacity);
    void Stop();

    bool Push(const LogRecord& record, uint64& sequence);
    bool Pop(LogRecord& record, uint32 timeout_ms = 0);
    uint64 LastSequence() const;
    void Complete(uint64 sequence);
    void Wait(uint64 sequence);

    void NotifyAll();
    uint32 Size() const;

private:
    uint32 m_capacity;
    bool m_running;
    uint64 m_next_sequence;
    uint64 m_completed_sequence;
    std::deque<LogRecord> m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv_not_empty;
    std::condition_variable m_cv_not_full;
    std::condition_variable m_cv_barrier;
};

}

#endif
