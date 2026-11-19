#ifndef COMM_LOG_FS_UTIL_H
#define COMM_LOG_FS_UTIL_H

#include "comm_types.h"
#include <string>
#include <vector>

namespace comm_log
{

struct FileStat
{
    std::string name;
    uint64 size;
};

class FsUtil
{
public:
    static bool EnsureDir(const std::string& dir, int mode);
    static bool ListFiles(const std::string& dir, std::vector<std::string>& names);
    static bool RemoveFile(const std::string& path);
    static bool StatFile(const std::string& path, FileStat& st);
};

}

#endif
