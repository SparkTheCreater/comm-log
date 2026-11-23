#include "internal/async_dispatcher.h"

namespace comm_log
{

AsyncDispatcher::AsyncDispatcher()
    : m_capacity(0)
    , m_running(false)
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

bool AsyncDispatcher::Push(const LogRecord& record)
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
