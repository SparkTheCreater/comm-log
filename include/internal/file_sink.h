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
    void Close();
    bool WriteLine(const std::string& line);
    void Flush();
    void FlushSync();

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
