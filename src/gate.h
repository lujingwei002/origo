#pragma once

#include "ae.h"
#include "config.h"
#include "logger.h"
#include <vector>
#include <map>

class Config;
class Server;
class UpstreamGroup;
class Upstream;

class Gate {
public:
    Gate();
    ~Gate();
public:
    int Main();
    int Forver();
public:
    UpstreamGroup* SelectUpstreamGroup(std::string& path);
    void RecvUpstreamData(Upstream* upstream, uint64_t sessionId, const char* data, size_t len);
    void RecvUpstreamKick(Upstream* upstream, uint64_t sessionId, const char* data, size_t len);
    void LogAccess(const char* fmt, ...);
    void LogError(const char* fmt, ...);
    void LogDebug(const char* fmt, ...);
    void LogAccess(const char* fmt, va_list args);
    void LogError(const char* fmt, va_list args);
    void LogDebug(const char* fmt, va_list args);
    void Shutdown();
    void Reload();
    int reload(Config* config);
    void UpstreamRemove(Upstream* upstream);
private:
    int initSignal();
    int initLogger();
    int initServer();
    int initUpstream();
    int initDaemon();
    int tryLockPid();
    int run();
    int addServer(ServerConfig& config);
    int removeServer(Server* server);
    int addUpstreamGroup(UpstreamGroupConfig& config);
    int removeUpstreamGroup(UpstreamGroup* upstreamGroup);
public:
    std::map<uint64_t, Server*>             serverDict;
    std::map<std::string, Server*>          name2Server;
    std::vector<UpstreamGroup*>             upstreamGroupArr;
    std::map<std::string, UpstreamGroup*>   upstreamGroupDict;
    Config*                                 config;
    aeEventLoop*                            loop;
    uint64_t                                serverId;
    Logger*                                 accessLogger;
    Logger*                                 errorLogger;
    Logger*                                 debugLogger;
    int                                     status;
    std::string                             configureFilePath;
};

Gate* NewGate();
extern Gate* gate;
