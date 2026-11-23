#ifndef COMM_LOG_FILE_SINK_H
#define COMM_LOG_FILE_SINK_H

#include "comm_types.h"

#include <stdio.h>
#include <string>

namespace comm_log
{

class FileSink
{
public:
    FileSink();
    ~FileSink();

    bool Open(const std::string& dir, const std::string& date_key);
    bool OpenPath(const std::string& path);
    bool OpenPath(const std::string& path, const std::string& date_key);
    bool Close();
    bool WriteLine(const std::string& line);
    bool Flush();
    bool FlushSync();
    bool IsOpen() const;

    const std::string& CurrentDateKey() const;
    const std::string& CurrentPath() const;

private:
    std::string m_dir;
    std::string m_date_key;
    std::string m_path;
    FILE* m_fp;
};

}

#endif
