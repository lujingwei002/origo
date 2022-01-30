#pragma once

#include "config.h"
#include "logger.h"

class Gate;
class Server;
class Upstream;

class Location {
public:
    Location(Gate* gate, Server* server, LocationConfig& config);
    ~Location();
public:
    void LogAccess(const char* fmt, va_list args);
    void LogError(const char* fmt, va_list args);
    void LogDebug(const char* fmt, va_list args);
    Upstream* SelectUpstream();
    int Start();
    int Reload(LocationConfig& config);
private:
    int initLogger();
public:
    Gate*           gate;
    Server*         server;
    LocationConfig  config;
    Logger*         accessLogger;
    Logger*         errorLogger;
};

Location* NewLocation(Gate* gate, Server* server, LocationConfig& config);
