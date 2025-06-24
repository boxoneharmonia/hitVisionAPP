#ifndef FILE_HPP
#define FILE_HPP

#include <mutex>
#include <string>
#include <fstream>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>

inline std::mutex fileMutex;
// inline std::mutex netMutex;
inline std::string folderPath;

inline std::string getExecutableDir()
{
    char path[PATH_MAX] = {0};
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len == -1)
    {
        throw std::runtime_error("Failed to read /proc/self/exe");
    }
    path[len] = '\0';
    return std::string(dirname(path));
}

inline void readIndexFile(int& index)
{
    std::lock_guard<std::mutex> lock(fileMutex);

    std::ifstream indexFile(folderPath + "/index.txt");
    index = 0;
    if (indexFile.is_open())
    {
        indexFile >> index;
        indexFile.close();
    }
}

#endif