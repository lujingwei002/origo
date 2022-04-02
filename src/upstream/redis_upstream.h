#pragma once 

#include "config.h"
#include "upstream.h"
#include "byte_array.h"
#include <netinet/in.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>

class Upstream;
class Gate;

class RedisUpstream : public UpstreamDriver {
public:
    RedisUpstream(Upstream* upstream);
    virtual ~RedisUpstream();
public:
    virtual int start();
    virtual void TryReconnect();
    virtual void Close();
    virtual void DelayClose();
    virtual void RecvClientData(const char* header, size_t headerLen, const char* payload, size_t len);
public:
    void evConnectError();
    void evConnectSucc();
public:
    Upstream*               upstream;
    byte_array*             recvBuffer;
    redisAsyncContext*      context;

};

RedisUpstream* NewRedisUpstream(Upstream* upstream);
