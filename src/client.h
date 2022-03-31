#pragma once

#include "byte_array.h"
#include "config.h"
#include "logger.h"
#include "json/json.h"
#include <cstdlib>
#include <cstdint>
#include <deque>

class Server;
class Gate;
class Upstream;
class Location;

class IClientHandler {
public:
    IClientHandler(){}
    virtual ~IClientHandler(){}
public:
    virtual int Unpack(const char* buffer, size_t len) = 0;
    virtual int Pack(const char* data, size_t len) = 0;
};

class Client {
public:
    Client(Gate* gate, Server* server, int sockfd, uint64_t sessionId);
    ~Client();
public:
    int start();
    void Close();
    void DelayClose();
    int Send(const char* data, size_t len);
    byte_array* WillSend(size_t len);
    void Recv(const char* data, size_t len);
    void recvUpstreamData(Upstream* upstream, const char* data, size_t len);
    void recvUpstreamKick(Upstream* upstream, const char* data, size_t len);
    void logAccess(const char* fmt, ...);
    void logError(const char* fmt, ...);
    void logDebug(const char* fmt, ...);
    void checkHeartbeat();
    void checkTimeout();
    void evRead();
    void evWrite();
    void onServerShutdown();
    int onUpstreamRemove(Upstream* upstream);
    int onLocationRemove(Location* location);
private:
    void onClose();
    void recvPakcetHandshake(const char* data, size_t len);
    void recvPakcetHandshakeAck(const char* data, size_t len);
    void recvPakcetData(const char* data, size_t len);
    void recvPakcetHeartbeat(const char* data, size_t len);
    void replyJson(uint8_t opcode, Json::Value& payload);
public:
    Gate*                   gate;
    Server*                 server;
    int                     sockfd;
    byte_array*             recvBuffer;
    IClientHandler*         handler;
    uint64_t                sessionId;
    std::deque<byte_array*> sendDeque;
    uint8_t                 status;
    int                     lastHeartbeatTime;
    int                     startTime;
    Upstream*               upstream;
    Location*               location;
    std::string             remoteAddr;
};

Client* NewClient(Gate* gate, Server* server, int sockfd, uint64_t sessionId);
