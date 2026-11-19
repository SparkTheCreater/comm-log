#include "internal/file_sink.h"

#include <stdio.h>
#include <unistd.h>

namespace comm_log
{

FileSink::FileSink()
    : m_fp(NULL)
{
}

FileSink::~FileSink()
{
    Close();
}

bool FileSink::Open(const std::string& dir, const std::string& date_key)
{
    Close();

    m_dir = dir;
    m_date_key = date_key;
    m_path = m_dir + "/" + m_date_key + ".log";

    m_fp = fopen(m_path.c_str(), "a");
    return (m_fp != NULL);
}

void FileSink::Close()
{
    if (m_fp)
    {
        fclose(m_fp);
        m_fp = NULL;
    }
    // fini 后必须清掉打开状态：EnsureOpenForDate 以 date_key+path 非空判定文件仍打开。
    // 若不清理，二次 init 会跳过 fopen，WriteLine 对着空指针丢日志。
    m_dir.clear();
    m_date_key.clear();
    m_path.clear();
}

bool FileSink::WriteLine(const std::string& line)
{
    if (!m_fp)
    {
        return false;
    }
    size_t n = fwrite(line.c_str(), 1, line.size(), m_fp);
    size_t m = fwrite("\n", 1, 1, m_fp);
    return (n == line.size() && m == 1);
}

void FileSink::Flush()
{
    if (m_fp)
    {
        fflush(m_fp);
    }
}

void FileSink::FlushSync()
{
    if (!m_fp)
    {
        return;
    }
    fflush(m_fp);
    int fd = fileno(m_fp);
    if (fd >= 0)
    {
        fsync(fd);
    }
}

const std::string& FileSink::CurrentDateKey() const
{
    return m_date_key;
}

const std::string& FileSink::CurrentPath() const
{
    return m_path;
}

}
