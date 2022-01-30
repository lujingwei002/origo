#pragma once

#include "config.h"
#include "byte_array.h"
#include "logger.h"

#include <vector>
#include <map>
#include <cstdint>

class ServerConfig;
class Location;
class Gate;
class Client;
class byte_array;
class UpstreamGroup;
class Upstream;

class Server {
public:
    Server(Gate* gate, uint64_t serverId, ServerConfig& config);
    ~Server();
public:
    int Start();
    byte_array* AllocBuffer(size_t size);
    void FreeBuffer(byte_array* b);
    int Time();
    Location* SelectLocation(std::string& path);
    void RecvUpstreamData(Upstream* upstream, uint64_t sessionId, const char* data, size_t len);
    void RecvUpstreamKick(Upstream* upstream, uint64_t sessionId, const char* data, size_t len);
    void LogAccess(const char* fmt, ...);
    void LogError(const char* fmt, ...);
    void LogDebug(const char* fmt, ...);
    void LogAccess(const char* fmt, va_list args);
    void LogError(const char* fmt, va_list args);
    void LogDebug(const char* fmt, va_list args);
    void Shutdown();
    int Reload(ServerConfig& config);
    void upstreamRemove(Upstream* upstream);
public:
    int initLogger();
    int initTimer();
    void onClose();
    void onAccept();
    void onClientClose(Client* client);
    void checkHeartbeat();
    void checkTimeout();
    int removeLocation(Location* location);
public:
    int                                 sockfd;
    Gate*                               gate;
    ServerConfig                        config;
    std::map<uint64_t, Client*>         clientDict;
    std::map<std::string, Location*>    locationDict;
    std::vector<byte_array*>            freeBufferArr;
    uint64_t                            serverId;
    Logger*                             accessLogger;
    Logger*                             errorLogger;
    int                                 lastAcceptTime;
    int                                 lastAcceptCnt;
    int                                 status;
    long long                           heartbeatTimerId;
    long long                           timeoutTimerId;
};

Server* NewServer(Gate* gate, uint64_t serverId, ServerConfig& config);
