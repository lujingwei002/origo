#pragma once

#include <vector>
#include <map>
#include <cstdint>
#include "byte_array.h"
#include "config.h"

class UpstreamConfig;
class Gate;
class Client;
class byte_array;
class Upstream;

class UpstreamGroup {
public:
    UpstreamGroup(Gate* gate, UpstreamGroupConfig& config);
    ~UpstreamGroup();
public:
    int Start();
    byte_array* AllocBuffer(size_t size);
    void FreeBuffer(byte_array* b);
    Upstream* SelectUpstream();
public:
    void TryReconnect();
    void TryHeartbeat();
    void Shutdown();
    void onUpstreamClose(Upstream* upstream);
    int initTimer();
    int Reload(UpstreamGroupConfig& config);
    int addUpstream(UpstreamConfig& config);
    int removeUpstream(Upstream* upstream);
public:
    Gate*                           gate;
    UpstreamGroupConfig             config;
    std::vector<Upstream*>          upstreamArr;
    std::map<std::string, Upstream*>upstreamDict;
    std::vector<byte_array*>        freeBufferArr;
    int                             status;
    long long                       heartbeatTimerId;
    long long                       reconnectTimerId;
};

UpstreamGroup* NewUpstreamGroup(Gate* gate, UpstreamGroupConfig& config);
