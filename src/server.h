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
class IClientHandler;
class byte_array;
class UpstreamGroup;
class Upstream;

class Server {
public:
    Server(Gate* gate, uint64_t serverId, ServerConfig& config);
    ~Server();
public:
    int start();
    byte_array* AllocBuffer(size_t size);
    void FreeBuffer(byte_array* b);
    int Time();
    Location* SelectLocation(std::string& path);
    void recvUpstreamData(Upstream* upstream, uint64_t sessionId, const char* data, size_t len);
    void recvUpstreamKick(Upstream* upstream, uint64_t sessionId, const char* data, size_t len);
    void logAccess(const char* fmt, ...);
    void logError(const char* fmt, ...);
    void logDebug(const char* fmt, ...);
    void logAccess(const char* fmt, va_list args);
    void logError(const char* fmt, va_list args);
    void logDebug(const char* fmt, va_list args);
    int shutdown();
    int reload(ServerConfig& config);
    void onUpstreamRemove(Upstream* upstream);
    void onClientClose(Client* client);
    IClientHandler* newHandler(Client* client);
public:
    void evAccept();
    void evTimeout();
    void evHeartbeat();
private:
    void onClose();
    int initLogger();
    int initTimer();
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
