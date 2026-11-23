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

    std::string path = dir + "/" + date_key + ".log";
    FILE* fp = fopen(path.c_str(), "a");
    if (!fp)
    {
        return false;
    }
    m_dir = dir;
    m_date_key = date_key;
    m_path = path;
    m_fp = fp;
    return true;
}

void FileSink::Close()
{
    if (m_fp)
    {
        fclose(m_fp);
        m_fp = NULL;
    }
    // 路径仅描述已成功打开的文件，不能作为文件描述符仍有效的依据。
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

bool FileSink::IsOpen() const
{
    return m_fp != NULL;
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
