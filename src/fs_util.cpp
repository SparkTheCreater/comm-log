#include "internal/fs_util.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

namespace comm_log
{

static bool IsDirectory(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static bool MkdirOne(const std::string& dir, int mode)
{
    if (dir.empty())
    {
        return false;
    }
    if (IsDirectory(dir))
    {
        return true;
    }
    if (mkdir(dir.c_str(), (mode_t)mode) == 0)
    {
        return true;
    }
    return errno == EEXIST && IsDirectory(dir);
}

bool FsUtil::EnsureDir(const std::string& dir, int mode)
{
    if (dir.empty())
    {
        return false;
    }
    std::string path;
    path.reserve(dir.size() + 1);

    for (size_t i = 0; i < dir.size(); ++i)
    {
        char c = dir[i];
        path.push_back(c);
        if (c == '/' && path.size() > 1)
        {
            if (!MkdirOne(path, mode))
            {
                return false;
            }
        }
    }
    if (!MkdirOne(path, mode))
    {
        return false;
    }
    return true;
}

bool FsUtil::ListFiles(const std::string& dir, std::vector<std::string>& names)
{
    names.clear();
    DIR* dp = opendir(dir.c_str());
    if (!dp)
    {
        return false;
    }

    struct dirent* ent = NULL;
    while ((ent = readdir(dp)) != NULL)
    {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
        {
            continue;
        }
        names.push_back(ent->d_name);
    }
    closedir(dp);
    return true;
}

bool FsUtil::RemoveFile(const std::string& path)
{
    return (unlink(path.c_str()) == 0);
}

bool FsUtil::StatFile(const std::string& path, FileStat& st)
{
    struct stat sb;
    if (stat(path.c_str(), &sb) != 0)
    {
        return false;
    }
    st.size = (uint64)sb.st_size;
    return true;
}

}
