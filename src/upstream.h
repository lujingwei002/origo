#pragma once

#include "byte_array.h"
#include "config.h"
#include <vector>
#include <map>
#include <deque>
#include <cstdint>

class UpstreamConfig;
class UpstreamGroup;
class Gate;
class Client;
class byte_array;

enum upstream_status {
    upstream_status_none        = 0,
    upstream_status_start       = 1,
    upstream_status_connecting  = 2,
    upstream_status_connect_err = 3,
    upstream_status_connect     = 4,
    upstream_status_closing     = 5,
    upstream_status_delayclose  = 6,
    upstream_status_closed      = 7,
};

class UpstreamDriver {
public:
    UpstreamDriver() {}
    virtual ~UpstreamDriver(){}
public:
    virtual int start() = 0;
    virtual void TryReconnect() = 0;
    virtual void Close() = 0;
    virtual void DelayClose() = 0;
    virtual void RecvClientData(const char* header, size_t headerLen, const char* payload, size_t len) = 0;
};

class Upstream {
public:
    Upstream(Gate* gate, UpstreamGroup* group, UpstreamConfig& config);
    ~Upstream();
public:
    int start();
    void Close();
    void DelayClose();
    void RecvClientNew(uint64_t sessionId);
    void RecvClientClose(uint64_t sessionId);
    void RecvClientData(uint64_t sessionId, const char* data, size_t len);
    int Unpack(const char* data, size_t len);
    void logDebug(const char* fmt, ...);
    void logError(const char* fmt, ...);
    int reload(UpstreamConfig& config);
public:
    void TryReconnect();
    void TryHeartbeat();
    void groupShutdown();
    void shutdown();
public:
    Gate*                   gate;
    UpstreamConfig          config;
    uint8_t                 status;
    UpstreamGroup*          group;
    UpstreamDriver*         driver;
};

Upstream* NewUpstream(Gate* gate, UpstreamGroup* group, UpstreamConfig& config);
