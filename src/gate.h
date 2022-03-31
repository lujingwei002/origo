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
    int main();
    void shutdown();
    int forver();
    void reload();
    int reload(Config* config);
    UpstreamGroup* selectUpstreamGroup(std::string& path);
    void recvUpstreamData(Upstream* upstream, uint64_t sessionId, const char* data, size_t len);
    void recvUpstreamKick(Upstream* upstream, uint64_t sessionId, const char* data, size_t len);
    void logAccess(const char* fmt, ...);
    void logError(const char* fmt, ...);
    void logDebug(const char* fmt, ...);
    void logAccess(const char* fmt, va_list args);
    void logError(const char* fmt, va_list args);
    void logDebug(const char* fmt, va_list args);
    void onUpstreamRemove(Upstream* upstream);
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
    aeEventLoop*                            loop;
    Config*                                 config;
private:
    std::map<uint64_t, Server*>             serverDict;
    std::map<std::string, Server*>          name2Server;
    std::vector<UpstreamGroup*>             upstreamGroupArr;
    std::map<std::string, UpstreamGroup*>   upstreamGroupDict;
    uint64_t                                serverId;
    Logger*                                 accessLogger;
    Logger*                                 errorLogger;
    Logger*                                 debugLogger;
    int                                     status;
    std::string                             configureFilePath;
};

Gate* NewGate();
extern Gate* gate;
