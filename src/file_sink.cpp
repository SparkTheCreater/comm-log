#include "internal/file_sink.h"

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

namespace comm_log
{

static bool RequireErrno(bool success)
{
    if (!success && errno == 0)
    {
        errno = EIO;
    }
    return success;
}

FileSink::FileSink()
    : m_size(0)
    , m_fp(NULL)
{
}

FileSink::~FileSink()
{
    Close();
}

bool FileSink::Open(const std::string& dir, const std::string& date_key)
{
    return OpenPath(dir + "/" + date_key + ".log", date_key);
}

bool FileSink::OpenPath(const std::string& path)
{
    return OpenPath(path, "");
}

bool FileSink::OpenPath(const std::string& path, const std::string& date_key)
{
    FILE* fp = fopen(path.c_str(), "a");
    if (!fp)
    {
        return false;
    }
    if (!Close())
    {
        fclose(fp);
        return false;
    }
    struct stat st;
    if (fstat(fileno(fp), &st) != 0)
    {
        fclose(fp);
        return false;
    }
    m_path = path;
    m_size = (uint64)st.st_size;
    m_date_key = date_key;
    m_fp = fp;
    return true;
}

bool FileSink::Close()
{
    int result = 0;
    if (m_fp)
    {
        result = fclose(m_fp);
        m_fp = NULL;
    }
    m_dir.clear();
    m_date_key.clear();
    m_path.clear();
    m_size = 0;
    return RequireErrno(result == 0);
}

bool FileSink::WriteLine(const std::string& line)
{
    if (!m_fp)
    {
        return false;
    }
    size_t n = fwrite(line.c_str(), 1, line.size(), m_fp);
    size_t m = fwrite("\n", 1, 1, m_fp);
    if (n == line.size() && m == 1)
    {
        m_size += n + m;
        return true;
    }
    if (errno == 0)
    {
        errno = EIO;
    }
    return false;
}

bool FileSink::Flush()
{
    return RequireErrno(!m_fp || fflush(m_fp) == 0);
}

bool FileSink::FlushSync()
{
    if (!m_fp || !Flush())
    {
        return false;
    }
    int fd = fileno(m_fp);
    if (fd < 0)
    {
        return false;
    }
    return RequireErrno(fsync(fd) == 0);
}

bool FileSink::IsOpen() const
{
    return m_fp != NULL;
}

uint64 FileSink::CurrentSize() const
{
    return m_size;
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
