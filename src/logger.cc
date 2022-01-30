#include "logger.h"
#include "errors.h"
#include <map>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

std::map<std::string, Logger*> loggerDict;

Logger* NewLogger(const char* filePath) {
    auto it = loggerDict.find(filePath);
    if (it != loggerDict.end()) {
        return it->second;
    }
    Logger* self = new Logger(filePath);
    return self;
}

Logger::Logger(const char* filePath) {
    this->format[0] = 0;
    this->file = nullptr;
    strcpy(this->filePath, filePath);
    this->maxFileSize = 2 * 1024 *1024 ;
    this->maxFileNum = 10;
}

Logger::~Logger() {
    auto it = loggerDict.find(this->filePath);
    if (it != loggerDict.end()) {
        loggerDict.erase(it);
    }
    this->Close();
}

void Logger::Log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    this->Log(fmt, args);
    va_end(args);
}

int Logger::getLogFileSize() {
    if (nullptr == this->file) {
        return -1;
    }
    struct stat buf;
    if (stat(this->filePath, &buf)) {
        return -1;
    }
    return buf.st_size;
}

void Logger::Close() {
    if (nullptr == this->file) {
        return;
    }
    fflush(this->file);
    fclose(this->file);
    this->file = nullptr;
}

bool Logger::isFileExist(const char* filePath) {
    if (access(filePath, F_OK) < 0) {
        return false;
    }
    return true;
}

int Logger::renameFile(const char* filePath, const char* newFilePath) {
    if (rename(filePath, newFilePath)) {
        return 1;
    }
    return 0;
}

int Logger::rorateFile() {
    static thread_local char path[PATH_MAX];
    static thread_local char tempPath[PATH_MAX];
    static thread_local char newPath[PATH_MAX];

    //  (1-9)=>(2-10).tmp
    for (int i = 1; i < this->maxFileNum; i++) {
        snprintf(path, PATH_MAX, "%s.%d", this->filePath, i); 
        if (!this->isFileExist(path)) {
            break;
        }
        snprintf(tempPath, PATH_MAX, "%s.%d_tmp", this->filePath, i+1); 
        int err = this->renameFile(path, tempPath);
        if (err) {
            return err;
        }
    }
    // (2-10).tmp => (2-10)
    for (int i = 2; i <= this->maxFileNum; i++) {
        snprintf(tempPath, PATH_MAX, "%s.%d_tmp", this->filePath, i); 
        if(this->isFileExist(tempPath)) {
            snprintf(newPath, PATH_MAX, "%s.%d", this->filePath, i); 
            int err = this->renameFile(tempPath, newPath);
            if (err) {
                return err;
            }
        }
    }

    this->Close();

    // => 1
    snprintf(newPath, PATH_MAX, "%s.%d", this->filePath, 1); 
    int err = this->renameFile(this->filePath, newPath); 
    if (err) {
        return err; 
    }

    FILE* file = fopen(this->filePath, "a");
    if (nullptr == file) {
        return 1;
    }
    setvbuf(file, (char*)0, _IOLBF, 0);
    this->file = file;
    return 0;
}

void Logger::Log(const char* fmt, va_list args) {
    if (nullptr == this->file) {
        return;
    }

    if (access(this->filePath, W_OK) < 0) {
        this->Close();
        return;
    }

    int fileSize = this->getLogFileSize();
    if (fileSize < 0) {
        this->Close();
        return;
    }

    if (fileSize >= this->maxFileSize) {
        if (this->rorateFile()) {
            return; 
        }
    }

    static thread_local char buffer[4096];
    time_t now;
    now = time(&now);
    struct tm date;
    localtime_r(&now, &date);
    size_t offset = 0;
    int n = 0;

    // 时间戳
    n = snprintf(buffer + offset, sizeof(buffer) - offset, "%02d-%02d-%02d %02d:%02d:%02d ", 
                date.tm_year + 1900, date.tm_mon+1, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec);
    if (n < 0) {
        return;
    }
    if (n >= (int)(sizeof(buffer) - offset)) {
        return;
    }
    offset += n;

    // 内容
    n = vsnprintf(buffer + offset, sizeof(buffer) - offset, fmt, args);
    if (n < 0) {
        return;
    }
    if (n >= (int)(sizeof(buffer) - offset)) {
        return;
    }
    offset += n;
    if (offset >= sizeof(buffer)) {
        return; 
    }
    // 换行
    buffer[offset] = '\n'; offset += 1;

    if(1 == fwrite(buffer, offset, 1, this->file)) {
        fflush(this->file);
    }
    fwrite(buffer, offset, 1, stdout);
}

int Logger::Start() {
    if (nullptr != this->file){
        return  1;
    }
    FILE* file = fopen(this->filePath, "a");
    if (nullptr == file) {
        throw exception(std::string("Can't Open File:")+this->filePath);
        return 1;
    }
    setvbuf(file, (char*)0, _IOLBF, 0);
    this->file = file;
    return 0;
}
