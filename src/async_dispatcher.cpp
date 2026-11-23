#include "internal/async_dispatcher.h"

namespace comm_log
{

AsyncDispatcher::AsyncDispatcher()
    : m_capacity(0)
    , m_running(false)
    , m_next_sequence(0)
    , m_completed_sequence(0)
{
}

AsyncDispatcher::~AsyncDispatcher()
{
    Stop();
}

bool AsyncDispatcher::Start(uint32 capacity)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_running)
    {
        return true;
    }
    m_capacity = capacity;
    m_running = true;
    return true;
}

void AsyncDispatcher::Stop()
{
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (!m_running)
        {
            return;
        }
        m_running = false;
    }
    NotifyAll();
}

bool AsyncDispatcher::Push(const LogRecord& record, uint64& sequence)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    while (m_running && m_queue.size() >= m_capacity)
    {
        m_cv_not_full.wait(lock);
    }
    if (!m_running)
    {
        return false;
    }
    sequence = ++m_next_sequence;
    m_queue.push_back(record);
    m_cv_not_empty.notify_one();
    return true;
}

bool AsyncDispatcher::Pop(LogRecord& record)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    while (m_running && m_queue.empty())
    {
        m_cv_not_empty.wait(lock);
    }
    if (m_queue.empty())
    {
        return false;
    }
    record = m_queue.front();
    m_queue.pop_front();
    m_cv_not_full.notify_one();
    return true;
}

uint64 AsyncDispatcher::LastSequence() const
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_next_sequence;
}

void AsyncDispatcher::Complete(uint64 sequence)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (sequence > m_completed_sequence)
    {
        m_completed_sequence = sequence;
    }
    m_cv_barrier.notify_all();
}

void AsyncDispatcher::Wait(uint64 sequence)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv_barrier.wait(lock, [this, sequence]()
    {
        return m_completed_sequence >= sequence;
    });
}

void AsyncDispatcher::NotifyAll()
{
    m_cv_not_empty.notify_all();
    m_cv_not_full.notify_all();
}

uint32 AsyncDispatcher::Size() const
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return (uint32)m_queue.size();
}

}
