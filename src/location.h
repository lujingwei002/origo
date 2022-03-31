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
    void logAccess(const char* fmt, va_list args);
    void logError(const char* fmt, va_list args);
    void logDebug(const char* fmt, va_list args);
    Upstream* SelectUpstream();
    int start();
    int reload(LocationConfig& config);
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
