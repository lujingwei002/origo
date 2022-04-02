#include "upstream/redis_upstream.h"
#include "gate.h"
#include "upstream_group.h"
#include "errors.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <hiredis/adapters/ae.h>

void ev_command(redisAsyncContext *c, void *r, void *privdata) {
    redisReply* reply = (redisReply*)r;
    if (reply == NULL) return;
    RedisUpstream* upstream = (RedisUpstream*)c->data;
    printf("argv[%s]: %s\n", (char*)privdata, reply->str);
}

void ev_connect_err(const redisAsyncContext *c, int status) {
    RedisUpstream* upstream = (RedisUpstream*)c->data;
    if (status != REDIS_OK) {
        upstream->evConnectError();
        return;
    }
    upstream->evConnectSucc();
}

void ev_connect_succ(const redisAsyncContext *c, int status) {
    RedisUpstream* upstream = (RedisUpstream*)c->data;
    if (status != REDIS_OK) {
        upstream->evConnectError();
        return;
    }
    upstream->evConnectError();
}

RedisUpstream* NewRedisUpstream(Upstream* upstream) {
    RedisUpstream* self = new RedisUpstream(upstream);
    return self;
}

RedisUpstream::RedisUpstream(Upstream* upstream) {
    this->upstream = upstream;
    this->recvBuffer = nullptr;
    this->context = nullptr;
}

RedisUpstream::~RedisUpstream() {
    Upstream* upstream = this->upstream;
    upstream->logDebug("[redis_upstream] ~RedisUpstream");
    if(this->recvBuffer) {
        upstream->group->FreeBuffer(this->recvBuffer);
        this->recvBuffer = nullptr;
    }
    if(this->context != nullptr) {
        redisAsyncFree(this->context);
        this->context = nullptr;
    }
}

void RedisUpstream::DelayClose() {
    Upstream* upstream = this->upstream;
    if (upstream->status == upstream_status_delayclose || upstream->status == upstream_status_closing || upstream->status == upstream_status_closed) {
        return;
    }
    redisAsyncDisconnect(this->context);
    upstream->status = upstream_status_delayclose;
}

void RedisUpstream::Close() {
    Upstream* upstream = this->upstream;
    if (upstream->status == upstream_status_closing || upstream->status == upstream_status_closed) {
        return;
    }
    redisAsyncDisconnect(this->context);
    upstream->status = upstream_status_closing;
}

void RedisUpstream::evConnectError() {
    Upstream* upstream = this->upstream;
    upstream->logDebug("[redis_upstream] evConnectError");
    if (upstream->status == upstream_status_connecting) {
        upstream->status = upstream_status_connect_err;
    } else if(upstream->status == upstream_status_closing) {
        upstream->status = upstream_status_closed;
    } else if(upstream->status == upstream_status_delayclose) {
        upstream->status = upstream_status_closed;
    } else {
        upstream->status = upstream_status_connect_err;
    }
    if (this->context) {
        redisAsyncFree(this->context);
        this->context = nullptr;
    }
}

void RedisUpstream::evConnectSucc() {
    Upstream* upstream = this->upstream;
    upstream->logDebug("[redis_upstream] evConnectSucc");
    redisAsyncCommand(this->context, ev_command, this, "AUTH %s", upstream->group->config.password.c_str());
    upstream->status = upstream_status_connect;
}

int RedisUpstream::start() {
    Upstream* upstream = this->upstream;
    if (upstream->status != upstream_status_none) {
        upstream->logError("[redis_upstream] start fail, status=%d, error='status error'", upstream->status);
        return e_status;
    }
    upstream->logError("[redis_upstream] start, host=%s, port=%d, weight=%d", upstream->config.host.c_str(), upstream->config.port, upstream->config.weight);
    byte_array* recvBuffer = upstream->group->AllocBuffer(upstream->group->config.recvBufferSize);
    if (recvBuffer == nullptr) {
        upstream->logError("[redis_upstream] start fail, error='alloc recv buffer'");
        return e_out_of_menory;
    }
    this->recvBuffer = recvBuffer;
    redisAsyncContext* c = redisAsyncConnect(upstream->config.host.c_str(), upstream->config.port);
    if (nullptr == c) {
        return e_socket;
    }
    c->data = this;
    this->context = c;
    redisAeAttach(upstream->gate->loop, c);
    redisAsyncSetConnectCallback(c, ev_connect_err);
    redisAsyncSetDisconnectCallback(c, ev_connect_succ);
    upstream->status = upstream_status_connecting;
    return 0;
}

void RedisUpstream::TryReconnect() {
    Upstream* upstream = this->upstream;
    if (upstream->status != upstream_status_connect_err && upstream->status != upstream_status_closed) {
        return;
    }
    //释放之前的资源
    if(this->recvBuffer) {
        this->recvBuffer->reset();
    }
    if(this->context != nullptr) {
        redisAsyncFree(this->context);
        this->context = nullptr;
    }
    upstream->logDebug("[redis_upstream] TryReconnect");

    redisAsyncContext* c = redisAsyncConnect(upstream->config.host.c_str(), upstream->config.port);
    if (nullptr == c) {
        upstream->logError("[redis_upstream] TryReconnect failed");
        return;
    }
    c->data = this;
    this->context = c;
    redisAeAttach(upstream->gate->loop, c);
    redisAsyncSetConnectCallback(c, ev_connect_err);
    redisAsyncSetDisconnectCallback(c, ev_connect_succ);
    upstream->status = upstream_status_connecting;
}

void RedisUpstream::RecvClientData(const char* header, size_t headerLen, const char* payload, size_t len){ 
    Upstream* upstream = this->upstream;
    size_t packetLen = headerLen + len;
    upstream->logDebug("[redis_upstream] RecvClientData packetLen:%ld", packetLen);
    if (upstream->status != upstream_status_connect) {
        upstream->logError("[redis_upstream] RecvClientData failed, error='not connect'");
        return;
    }
    if (nullptr == this->context) {
        upstream->logError("[redis_upstream] RecvClientData failed, error='context not found'");
        return;
    }
    printf("ggggggggggggg\n");
    redisAsyncCommand(this->context, ev_command, this, "PUBLISH aaa %b%b", header, headerLen, payload, len);
    return;
}

