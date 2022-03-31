#include "server.h"
#include "gate.h"
#include "ae.h"
#include "errors.h"
#include "config.h"
#include "client.h"
#include "byte_array.h"
#include "upstream_group.h"
#include "upstream.h"
#include "location.h"
#include "client/telnet_client.h"
#include "client/websocket_client.h"
#include "client/console_client.h"
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

enum server_status {
    server_status_none          = 0,
    server_status_start         = 1,
    server_status_closing       = 2,
    server_status_closed        = 3,
};

static int _evHeartbeatProc(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    Server* server = (Server*)clientData;
    server->evHeartbeat();
    return server->config.heartbeat * 1000;
}

static int _evTimeoutProc(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    Server* server = (Server*)clientData;
    server->evTimeout();
    return server->config.timeout * 1000;
}

static void _aeEventFinalizerProc(struct aeEventLoop *eventLoop, void *clientData) {

}

static void on_accept(struct aeEventLoop *eventLoop, int listenfd, void *args, int event) {
    Server* server = (Server*)args;
    server->evAccept();
}

Server* NewServer(Gate* gate, uint64_t serverId, ServerConfig& config) {
    Server* self = new Server(gate, serverId, config);
    return self;
}

Server::Server(Gate* gate, uint64_t serverId, ServerConfig& config) {
    this->gate = gate;
    this->config = config;
    this->sockfd = -1;
    this->serverId = serverId;
    this->accessLogger = nullptr;
    this->errorLogger = nullptr;
    this->lastAcceptTime = 0;
    this->lastAcceptCnt = 0;
    this->status = server_status_none;
    this->timeoutTimerId = -1;
    this->heartbeatTimerId = -1;
}

Server::~Server() {
    this->logDebug("[Server] ~Server");
    if(this->accessLogger) {
        this->accessLogger = nullptr;
    }
    if(this->errorLogger) {
        this->errorLogger = nullptr;
    }
    for (auto& it : this->locationDict) {
        delete it.second;
    }
    this->locationDict.clear();
    for (auto& it : this->freeBufferArr) {
        delete it;
    }
    this->freeBufferArr.clear();
    for (auto& it : this->clientDict) {
        delete it.second;
    }
    this->clientDict.clear();
    if(this->sockfd != -1) {
        close(this->sockfd);
        aeDeleteFileEvent(this->gate->loop, sockfd, AE_READABLE);
        this->sockfd = -1;
    }
    if (this->heartbeatTimerId >= 0) {
        aeDeleteTimeEvent(this->gate->loop, this->heartbeatTimerId);
        this->heartbeatTimerId = -1;
    }
    if (this->timeoutTimerId >= 0) {
        aeDeleteTimeEvent(this->gate->loop, this->timeoutTimerId);
        this->timeoutTimerId = -1;
    }
}

void Server::evAccept() {
    int sockfd;
    struct sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);
    sockfd = accept(this->sockfd, (struct sockaddr*)&addr, &addrLen);
    if (sockfd == -1) {
        return;
    }
    const char* remoteAddr = inet_ntoa(addr.sin_addr);

    this->logAccess("[server] accept a client from %s", remoteAddr);
    // 每秒接受的链接数
    if (this->lastAcceptCnt > this->config.maxConnPerSec) {
        this->logError("[server] %s max conn per sec reach'", remoteAddr);
        close(sockfd);
        return;
    }
    if (this->Time() == this->lastAcceptTime) {
        this->lastAcceptCnt++;
    } else {
        this->lastAcceptTime = this->Time();
        this->lastAcceptCnt = 1;
    }
    // 分配sessionid
    time_t timep;
    struct tm *p;
    time(&timep);
    p = localtime(&timep); /* 取得当地时间*/
    uint64_t now = p->tm_hour*10000+p->tm_min*100+p->tm_sec;
    uint64_t sessionId = sockfd;
    sessionId = (this->serverId << 50) | (now << 32) | sessionId;

    Client* client = NewClient(this->gate, this, sockfd, sessionId);
    if (client == nullptr) {
        this->logError("[server] out of memory when accept");
        return;
    }
    int err = client->start();
    if (err) {
        delete client;
        this->logError("[server] client start failed, error=%d'", err);
        return;
    }
    this->clientDict[sessionId] = client;
}

int Server::initLogger() {
    if (this->accessLogger == nullptr && this->config.accessLog.length() > 0) {
        Logger* accessLogger = NewLogger(this->config.accessLog.c_str());
        if (nullptr == accessLogger) {
            return e_out_of_menory;
        }
        int err = accessLogger->start();
        if (err) {
            delete accessLogger;
            return err;
        }
        this->accessLogger = accessLogger;
    }
    if (this->errorLogger == nullptr && this->config.errorLog.length() > 0) {
        Logger* errorLogger = NewLogger(this->config.errorLog.c_str());
        if (nullptr == errorLogger) {
            return e_out_of_menory;
        }
        int err = errorLogger->start();
        if (err) {
            delete errorLogger;
            return err;
        }
        this->errorLogger = errorLogger;
    }
    return 0;
}

int Server::initTimer() {
    if (this->heartbeatTimerId < 0 && this->config.heartbeat > 0) {
        this->heartbeatTimerId = aeCreateTimeEvent(this->gate->loop, this->config.heartbeat * 1000, _evHeartbeatProc, this, _aeEventFinalizerProc);
        if (this->heartbeatTimerId == AE_ERR) {
            return e_ae_createtimer;
        }
    }
    if (this->timeoutTimerId < 0 && this->config.timeout > 0) {
        this->timeoutTimerId = aeCreateTimeEvent(this->gate->loop, this->config.timeout * 1000, _evTimeoutProc, this, _aeEventFinalizerProc);
        if (this->timeoutTimerId == AE_ERR) {
            return e_ae_createtimer;
        }
    }
    return 0;
}

int Server::start() {
    int16_t port = this->config.port;
    int err = this->initLogger();
    if (err) {
        return err;
    }
    err = this->initTimer();
    if (err) {
        return err;
    }
    this->logDebug("[server] start, id=%ld, port=%d", this->serverId, this->config.port);
    for (auto& it : this->config.locationDict) {
        Location* location = NewLocation(this->gate, this, it.second);
        int err = location->start();
        if (err) {
            delete location;
            return err;
        }
        this->logDebug("[server] add location, path=%s", location->config.path.c_str());
        this->locationDict[location->config.path] = location;
    }
    int sockfd;
    struct sockaddr_in addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return e_socket;
    }
    bzero((void*)&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return e_bind;
    }
    if (listen(sockfd, 1024) < 0) {
        return e_listen;
    }
    if (AE_ERR == aeCreateFileEvent(this->gate->loop, sockfd, AE_READABLE, on_accept, (void*)this)) {
        return e_ae;
    }
    this->sockfd = sockfd;
    this->status = server_status_start;
    return 0;
}

void Server::onClose() {
}

int Server::shutdown() {
    if(this->status != server_status_start) {
        delete this;
        return e_server_status;
    }
    this->status = server_status_closing;
    for (auto it : this->clientDict) {
        it.second->onServerShutdown();
    }
    return 0;
}

void Server::onClientClose(Client* client) {
    this->logDebug("[server] onClientClose");
    auto it = this->clientDict.find(client->sessionId);
    if (it == this->clientDict.end()) {
        this->logError("[server] onClientClose, error='client not found'");
        return;
    }
    this->clientDict.erase(it);
    delete it->second;
    if (this->status == server_status_closing) {
        if (this->clientDict.size() <= 0) {
            delete this;
        }
    }
}

byte_array* Server::AllocBuffer(size_t size) {
    if(this->freeBufferArr.size() <= 0) {
        byte_array* b = new byte_array(size);
        if(b == nullptr) {
            return nullptr;
        }
        this->freeBufferArr.push_back(b);
    }
    byte_array* b = this->freeBufferArr.back();
    b->reset();
    this->freeBufferArr.pop_back();
    return b;
}

void Server::FreeBuffer(byte_array* b) {
    this->freeBufferArr.push_back(b);
}

int Server::Time() {
    return time(NULL);
}

Location* Server::SelectLocation(std::string& path) {
    auto it = this->locationDict.find(path);
    if (it == this->locationDict.end()) {
        return nullptr;
    }
    return it->second;
}

void Server::recvUpstreamData(Upstream* upstream, uint64_t sessionId, const char* data, size_t len) {
    auto it = this->clientDict.find(sessionId);
    if (it == this->clientDict.end()) {
        this->logError("[server] %ld session not found when recv upstream data, %s %s", sessionId, upstream->group->config.name.c_str(), upstream->config.name.c_str());
        return;
    }
    it->second->recvUpstreamData(upstream, data, len); 
}

void Server::recvUpstreamKick(Upstream* upstream, uint64_t sessionId, const char* data, size_t len) {
    auto it = this->clientDict.find(sessionId);
    if (it == this->clientDict.end()) {
        this->logError("[server] %ld session not found when recv upstream kick, %s %s", sessionId, upstream->group->config.name.c_str(), upstream->config.name.c_str());
        return;
    }
    it->second->recvUpstreamKick(upstream, data, len); 
}

int Server::reload(ServerConfig& config) {
    this->config.maintain = config.maintain;
    this->config.recvBufferSize = config.recvBufferSize;
    this->config.sendBufferSize = config.sendBufferSize;
    this->config.requirepass = config.requirepass;
    this->config.maxConnPerSec = config.maxConnPerSec;
    // 重载timer
    if (this->config.heartbeat != config.heartbeat) {
        this->config.heartbeat = config.heartbeat;
        if(this->heartbeatTimerId >= 0) {
            aeDeleteTimeEvent(this->gate->loop, this->heartbeatTimerId);
            this->heartbeatTimerId = -1;
        }
    }
    if (this->config.timeout != config.timeout) {
        this->config.timeout = config.timeout;
        if (this->timeoutTimerId >= 0) {
            aeDeleteTimeEvent(this->gate->loop, this->timeoutTimerId);
            this->timeoutTimerId = -1;
        }
    }
    int err = this->initTimer();
    if (err) {
        return err;
    }
    // 重载logger
    if (this->config.accessLog != config.accessLog && this->accessLogger != nullptr) {
        delete this->accessLogger; 
        this->accessLogger = nullptr;
        this->config.accessLog = config.accessLog;
    }
    if (this->config.errorLog != config.errorLog && this->errorLogger != nullptr) {
        delete this->errorLogger; 
        this->errorLogger = nullptr;
        this->config.errorLog = config.errorLog;
    }
    err = this->initLogger();
    if (err) {
        return err;
    }
    // 重载location
    for (auto& it : config.locationDict) {
        if (this->locationDict.find(it.second.path) == this->locationDict.end()) {
            Location* location = NewLocation(this->gate, this, it.second);
            int err = location->start();
            if (err) {
                delete location;
                return err;
            }
            this->logDebug("[server] add location, path=%s", location->config.path.c_str());
            this->locationDict[location->config.path] = location;
        }
    }
    std::vector<Location*> removeLocationArr;
    for (auto& it : this->locationDict) {
        const auto& c = config.locationDict.find(it.second->config.path);
        if (c  == config.locationDict.end()) {
            removeLocationArr.push_back(it.second);
        } else {
            it.second->reload(c->second);
        }
    }
    for (auto& it : removeLocationArr) {
        this->removeLocation(it);
    }
    return 0;
}

void Server::logAccess(const char* fmt, ...) {
    if(nullptr != this->accessLogger) {
        va_list args;
        va_start(args, fmt);
        this->accessLogger->Log(fmt, args);
        va_end(args);
        return;
    }
    va_list args;
    va_start(args, fmt);
    this->gate->logAccess(fmt, args);
    va_end(args);
}

void Server::logError(const char* fmt, ...) {
    if(nullptr != this->errorLogger) {
        va_list args;
        va_start(args, fmt);
        this->errorLogger->Log(fmt, args);
        va_end(args);
        return;
    }
    va_list args;
    va_start(args, fmt);
    this->gate->logError(fmt, args);
    va_end(args);
}

void Server::logDebug(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    this->gate->logDebug(fmt, args);
    va_end(args);
}

void Server::logAccess(const char* fmt, va_list args) {
    if(nullptr != this->accessLogger) {
        this->accessLogger->Log(fmt, args);
        return;
    }
    this->gate->logAccess(fmt, args);
}

void Server::logError(const char* fmt, va_list args) {
    if(nullptr != this->errorLogger) {
        this->errorLogger->Log(fmt, args);
        return;
    }
    this->gate->logError(fmt, args);
}

void Server::logDebug(const char* fmt, va_list args) {
    this->gate->logDebug(fmt, args);
}

void Server::evHeartbeat() {
    for (auto it : this->clientDict) {
        it.second->checkHeartbeat();
    }
}

void Server::evTimeout() {
    for (auto it : this->clientDict) {
        it.second->checkTimeout();
    }
}

int Server::removeLocation(Location* location) {
    this->logDebug("[server] remove location, path=%s", location->config.path.c_str());
    for (auto it : this->clientDict) {
        it.second->onLocationRemove(location);
    }
    this->locationDict.erase(location->config.path);
    delete location;
    return 0;
}

void Server::onUpstreamRemove(Upstream* upstream) {
    for (auto it : this->clientDict) {
        it.second->onUpstreamRemove(upstream);
    }
}

IClientHandler* Server::newHandler(Client* client) {
    if (this->config.net == "ws") {
        return NewWebsocketClient(this->gate, this, client);
    } else if (this->config.net == "telnet") {
        return NewTelnetClient(this->gate, this, client);
    } else if (this->config.net == "ctl") {
        return NewConsoleClient(this->gate, this, client);
    }
    return nullptr;
}


