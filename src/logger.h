#pragma once

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <limits.h> //PATH_MAX
#include <cstdarg>

class Logger {
public:
    Logger(const char* filePath);
    ~Logger();
public:
    int start();
    void Log(const char* fmt, ...);
    void Log(const char* fmt, va_list args);
    void Close();
public:
    int getLogFileSize();
    int rorateFile();
    bool isFileExist(const char* filePath);
    int renameFile(const char* filePath, const char* newFilePath);
private:
    char format[512];
    char filePath[PATH_MAX];
    FILE* file;
    int     maxFileSize;
    int     maxFileNum;
};

Logger* NewLogger(const char* filePath);
