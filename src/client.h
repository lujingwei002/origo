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
    int Start();
    void Close();
    void DelayClose();
    int Send(const char* data, size_t len);
    byte_array* WillSend(size_t len);
    void Recv(const char* data, size_t len);
    void RecvUpstreamData(Upstream* upstream, const char* data, size_t len);
    void RecvUpstreamKick(Upstream* upstream, const char* data, size_t len);
    void LogAccess(const char* fmt, ...);
    void LogError(const char* fmt, ...);
    void LogDebug(const char* fmt, ...);
public:
    void onClose();
    void onRead();
    void onWrite();
    void recvPakcetHandshake(const char* data, size_t len);
    void recvPakcetHandshakeAck(const char* data, size_t len);
    void recvPakcetData(const char* data, size_t len);
    void recvPakcetHeartbeat(const char* data, size_t len);
    void replyJson(uint8_t opcode, Json::Value& payload);
    void checkHeartbeat();
    void checkTimeout();
    void serverShutdown();
    int locationRemove(Location* location);
    int upstreamRemove(Upstream* upstream);
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
