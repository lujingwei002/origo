#include "client.h"
#include "gate.h"
#include "server.h"
#include "errors.h"
#include "config.h"
#include "location.h"
#include "handler/telnet_handler.h"
#include "handler/websocket_handler.h"
#include "upstream_group.h"
#include "upstream.h"
#include "json/json.h"
#include <cstdio>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <malloc.h>
#include <iostream>

/*

 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-------+---------------+-------------------------------+
|F|R|R|R| Opcode|   Reserve     |    Payload Data               |
|I|S|S|S|  (4)  |     (8)       |                               |
|N|V|V|V|       |               |                               |
| |1|2|3|       |               |                               |
+-+-+-+-+-------+---------------+ - - - - - - - - - - - - - - - +
|                     Payload Data                              | 
+-------------------------------- - - - - - - - - - - - - - - - +
:                     Payload Data continued ...                :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
|                     Payload Data continued ...                |
+---------------------------------------------------------------+

```json
handshake response:
{
    "code":200,
}
```
##code
200: OK
400: BAD REQUEST
401  Unauthorized
402  Payment Required
403  Forbidden
404  NOT FOUND

*/
class Server;
class Agent;

enum client_status {
    client_status_none          = 0,
    client_status_start         = 1,
    client_status_handshake     = 2,
    client_status_working       = 3,
    client_status_delayclose    = 4,
    client_status_closing       = 5,
    client_status_closed        = 6,
};

enum packet_type {
    packet_type_handshake       = 1,
    packet_type_handshake_ack   = 2,
    packet_type_data            = 3,
    packet_type_heartbeat       = 4,
    packet_type_kick            = 5,
    packet_type_down            = 6,
    packet_type_maintain        = 7,
};

struct packet_header {
    uint8_t opcode:4;
    uint8_t rsv:3;
    uint8_t fin:1;
    uint8_t reserve;
};

static void on_read(struct aeEventLoop *eventLoop, int sockfd, void *args, int event) {
    Client* client = (Client*)args;
    client->onRead();
}

static void on_write(struct aeEventLoop *eventLoop, int sockfd, void *args, int event) {
    Client* client = (Client*)args;
    client->onWrite();
}

Client* NewClient(Gate* gate, Server* server, int sockfd, uint64_t sessionId) {
    Client* self = new Client(gate, server, sockfd, sessionId);
    return self;
}

Client::Client(Gate* gate, Server* server, int sockfd, uint64_t sessionId) {
    this->gate = gate;
    this->server = server;
    this->sockfd = sockfd;
    this->sessionId = sessionId;
    this->recvBuffer = nullptr;
    this->handler = nullptr;
    this->lastHeartbeatTime = 0;
    this->upstream = nullptr;
    this->location = nullptr;
    this->status = client_status_none;
}

Client::~Client() {
    this->LogDebug("[client] ~Client");
    if(this->recvBuffer) {
        this->server->FreeBuffer(this->recvBuffer);
        this->recvBuffer = nullptr;
    }
    if(this->handler) {
        delete this->handler;
        this->handler = nullptr;
    }
    while(this->sendDeque.size() > 0) {
        byte_array* b = this->sendDeque.back();
        this->sendDeque.pop_back();
        b->reset();
        this->server->FreeBuffer(b);
    }
    if(this->sockfd != -1) {
        aeDeleteFileEvent(this->gate->loop, this->sockfd, AE_READABLE | AE_WRITABLE);
        close(this->sockfd);
        this->sockfd = -1;
    }
}

void Client::serverShutdown() {
    Json::Value response;
    response["code"] = 0;
    this->replyJson(packet_type_down, response);
    this->DelayClose();
}

void Client::DelayClose() {
    if (this->status == client_status_delayclose || this->status == client_status_closing || this->status == client_status_closed) {
        return;
    }
    this->status = client_status_delayclose;
    aeDeleteFileEvent(this->gate->loop, this->sockfd, AE_READABLE);
    //只关闭读
    shutdown(this->sockfd,SHUT_RD); 
}

void Client::Close() {
    if (this->status == client_status_closing || this->status == client_status_closed) {
        return;
    }
    this->status = client_status_closing;
    shutdown(this->sockfd, SHUT_RDWR); 
}

void Client::onClose() {
    this->LogDebug("[client] onClose");
    if (this->status == client_status_delayclose) {
        //关闭写
        shutdown(this->sockfd, SHUT_RDWR);
    } else if (this->status == client_status_closing) {
        
    }
    if (this->upstream != nullptr) {
        this->LogAccess("| %15s | %ld | %ld | G=>S | close |", this->remoteAddr.c_str(), this->server->serverId, this->sessionId);
        this->upstream->RecvClientClose(this->sessionId);
    }
    this->server->onClientClose(this);
    this->status = client_status_closed;
}

void Client::onWrite() {
    for(;;) {
        if (this->sendDeque.size() <= 0) {
            if (this->status == client_status_delayclose) {
                this->onClose();
                break;
            } else {
                aeDeleteFileEvent(this->gate->loop, this->sockfd, AE_WRITABLE);
                break;
            }
        }
        byte_array* b = this->sendDeque.back();
        if (nullptr == b) {
            if (this->status == client_status_delayclose) {
                this->onClose();
                break;
            } else {
                break;
            }
        }
        if (b->length() <= 0) {
            this->sendDeque.pop_back();
            aeDeleteFileEvent(this->gate->loop, this->sockfd, AE_WRITABLE);
            break;
        }
        int ir = send(this->sockfd, b->front(), b->length(), 0);
        this->LogDebug("[client] onWrite,len:%ld, result= %d", b->length(), ir);
        if (ir > 0) {
            b->read(ir);
            if (b->length() <= 0) {
                this->sendDeque.pop_back();
                this->server->FreeBuffer(b);
            }
        } else if (ir == -1 && errno == EAGAIN) {
            break;
        } else if (ir == -1) {
            //this->Close();
            break;
        }
    }
}

// |buffer  begin  end capacity |
void Client::onRead() {
    for(;;) {
        int ir = recv(this->sockfd, (void*)this->recvBuffer->back(), this->recvBuffer->capacity(), 0); 
        int last_errno = errno;
        this->LogDebug("[client] onRead, recv=%d, error=%d %s", ir, errno, strerror(errno));
        if (ir == 0 || (ir == -1 && last_errno != EAGAIN)) {
            //close
            this->onClose();
            return;
        }
        if (ir == -1 && last_errno == EAGAIN) {
            this->LogDebug("[client] onRead, no more data, wait");
            //wait data
            break;
        }
        if (this->handler == nullptr) {
            this->LogDebug("[client] onRead, handler not found, discard data");
            //discard data and wait
            continue;
        }
        this->recvBuffer->write(ir);
        for(;;) {
            int r = this->handler->Unpack(this->recvBuffer->front(), this->recvBuffer->length());
            if (r > 0) {
                this->recvBuffer->read(r);
            } else if(r == 0) {
                //more data
                if (this->recvBuffer->front() == this->recvBuffer->data() && this->recvBuffer->back() == this->recvBuffer->end()) {
                    this->LogDebug("[client] onRead failed, error='buffer not enough'");
                    this->Close();
                    return;
                } else {
                    this->recvBuffer->truncate();
                    break;
                }
            }
        }
        if (this->status == client_status_delayclose) {
            return;
        }
    }
}

int Client::Start() {
    if (this->status != client_status_none) {
        return e_status;
    }
    struct sockaddr_in addr;    
    socklen_t addrLen = sizeof(addr);   
    getpeername(this->sockfd, (struct sockaddr*)&addr, &addrLen);
    this->remoteAddr = inet_ntoa(addr.sin_addr);

    this->LogAccess("| %15s | %ld | %ld | accept a client", 
                this->remoteAddr.c_str(), this->server->serverId, this->sessionId);

    int flags = fcntl(this->sockfd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(this->sockfd, F_SETFL, flags);

    byte_array* recvBuffer = this->server->AllocBuffer(this->server->config.recvBufferSize);
    if (recvBuffer == nullptr) {
        this->LogDebug("[client] Start fail, error='alloc recv buffer'");
        return 1;
    }
    this->recvBuffer = recvBuffer;
    if (this->server->config.handshake) {
        this->status = client_status_start;
    } else {
        this->status = client_status_working;
        this->lastHeartbeatTime = this->server->Time();
    }
    this->startTime = this->server->Time();
    if (this->server->config.net == "ws") {
        this->handler = NewWebsocketHandler(this->gate, this->server, this);
    } else if (this->server->config.net == "telnet") {
        this->handler = NewTelnetHandler(this->gate, this);
    }
    aeCreateFileEvent(this->gate->loop, this->sockfd, AE_READABLE, on_read, this);
    aeCreateFileEvent(this->gate->loop, this->sockfd, AE_WRITABLE, on_write, this);
    return 0;
}

int Client::Send(const char* data, size_t len) {
    if (nullptr == this->handler) {
        return -1;
    }
    int err = this->handler->Pack(data, len);
    if (err) {
        return err;
    }
    return 0;
}

byte_array* Client::WillSend(size_t len) {
    if(this->sendDeque.size() <= 0 || this->sendDeque.front()->capacity() < len) {
        if (len > this->server->config.sendBufferSize) {
            this->LogError("[client] send buffer overflow, config=%ld, expect=%ld", this->server->config.sendBufferSize, len);
            return nullptr;
        } 
        byte_array* b = this->server->AllocBuffer(this->server->config.sendBufferSize);
        this->sendDeque.push_front(b);
        aeCreateFileEvent(this->gate->loop, this->sockfd, AE_WRITABLE, on_write, this);
        return b;
    } else {
        aeCreateFileEvent(this->gate->loop, this->sockfd, AE_WRITABLE, on_write, this);
        return this->sendDeque.front();
    }
}

void Client::Recv(const char* data, size_t len) {
    packet_header* header = (packet_header*)data;
    switch(header->opcode) {
        case packet_type_handshake:{
            this->recvPakcetHandshake(data, len);
        }break;
        case packet_type_handshake_ack: {
            this->recvPakcetHandshakeAck(data, len);
        }break;
        case packet_type_data: {
            this->recvPakcetData(data, len);
        }break;
        case packet_type_heartbeat: {
            this->recvPakcetHeartbeat(data, len);
        }break;
        default: {
            this->LogDebug("[client] recv a packet, type=%d", header->opcode);
        }break; 
    }
    //this->Send("World", 5);
}

void Client::replyJson(uint8_t opcode, Json::Value& payload) {
    static thread_local char buffer[1024];

    packet_header* header = (packet_header*)buffer;
    header->opcode = opcode;
    Json::FastWriter writer;
    std::string data = writer.write(payload);
    memcpy(buffer + sizeof(packet_header), data.c_str(), data.length());
    this->Send(buffer, sizeof(packet_header) + data.length());

    switch(opcode) {
        case packet_type_handshake:{
            this->LogAccess("| %15s | %ld | %ld | C<=G | handshake | %s", 
                this->remoteAddr.c_str(), this->server->serverId, this->sessionId, data.c_str());
        }break;
        case packet_type_down:{
            this->LogAccess("| %15s | %ld | %ld | C<=G | down | %s", 
                this->remoteAddr.c_str(), this->server->serverId, this->sessionId, data.c_str());
        }break;
    }
}

void Client::recvPakcetHandshake(const char* data, size_t len) {

    this->LogDebug("[client] recv a packet, type=handshake");
    if (this->status != client_status_start) {
        this->LogError("[client] recvPakcetData failed, error='status error, not start'");
        Json::Value response;
        response["code"] = 400;
        this->replyJson(packet_type_handshake, response);
        return;
    }
    if (this->gate->config->maintain.length() > 0) {
        Json::Value response;
        response["code"] = 503;
        response["msg"] = this->gate->config->maintain;
        this->replyJson(packet_type_handshake, response);
        this->DelayClose();
        return;
    }
    if (this->server->config.maintain.length() > 0) {
        Json::Value response;
        response["code"] = 503;
        response["msg"] = this->server->config.maintain;
        this->replyJson(packet_type_handshake, response);
        this->DelayClose();
        return;
    }
    const char* payload = (char*)data + sizeof(packet_header);
    Json::Reader reader;
    Json::Value root;
    // reader将Json字符串解析到root，root将包含Json里所有子元素  
    if (!reader.parse(payload, payload + len - sizeof(packet_header), root)) {
        this->LogError("[client] recvPakcetHandshake failed, error='json parse'");
        Json::Value response;
        response["code"] = 400;
        this->replyJson(packet_type_handshake, response);
        return;
    }
    std::string path = root["path"].asString();
    std::string password = root["password"].asString();
    this->LogDebug("[client] recv a packet, type=handshake, path=%s", path.c_str());
    if (this->server->config.requirepass.length() > 0 && this->server->config.requirepass != password) {
        this->LogError("[client] recvPakcetHandshake failed, password=%s, error='password invalid'", password.c_str());
        Json::Value response;
        response["code"] = 401;
        this->replyJson(packet_type_handshake, response);
        return;
    }
    Location* location = this->server->SelectLocation(path);
    if (nullptr == location) {
        this->LogError("[client] recvPakcetHandshake failed, path=%s, error='location not found'", path.c_str());
        Json::Value response;
        response["code"] = 404;
        this->replyJson(packet_type_handshake, response);
        return;
    }
    Upstream* upstream = location->SelectUpstream();
    if (nullptr == upstream) {
        this->LogError("[client] recvPakcetHandshake failed, path=%s, error='upstream not found'", path.c_str());
        Json::Value response;
        response["code"] = 404;
        this->replyJson(packet_type_handshake, response);
        return;
    }
    this->location = location;
    this->upstream = upstream;
    this->status = client_status_handshake;

    this->LogAccess("| %15s | %ld | %ld | C=>G | handshake |", 
                this->remoteAddr.c_str(), this->server->serverId, this->sessionId);
    Json::Value response;
    response["code"] = 200;
    response["heartbeat"] = this->server->config.heartbeat;
    this->replyJson(packet_type_handshake, response);
}

void Client::recvPakcetHandshakeAck(const char* data, size_t len) { 

    this->LogAccess("| %15s | %ld | %ld | C=>G | handshake ack |", 
                this->remoteAddr.c_str(), this->server->serverId, this->sessionId);

    this->LogDebug("[client] recv a packet, type=handshake ack");
    if (this->status != client_status_handshake) {
        this->LogError("[client] recvPakcetHandshakeAck failed, status=%d, error='status error, handshake expect'", this->status);
        return;
    }
    if (this->upstream != nullptr) {
        this->LogAccess("| %15s | %ld | %ld | G=>S | new |", 
                this->remoteAddr.c_str(), this->server->serverId, this->sessionId);
        this->upstream->RecvClientNew(this->sessionId);
    }
    this->lastHeartbeatTime = this->server->Time();
    this->status = client_status_working;
}

void Client::recvPakcetData(const char* data, size_t len) { 

    this->LogAccess("| %15s | %ld | %ld | C=>G | data |", 
                this->remoteAddr.c_str(), this->server->serverId, this->sessionId);

    this->LogDebug("[client] recv a packet, type=data");
    if (this->status != client_status_working) {
        this->LogError("[client] recvPakcetData failed, status=%d, error='status error, working expect'", this->status);
        return;
    }
    this->lastHeartbeatTime = this->server->Time();
    const char* payload = (char*)(data) + sizeof(packet_header);
    if (this->upstream != nullptr) {
        this->LogAccess("| %15s | %ld | %ld | G=>S | data |", 
                this->remoteAddr.c_str(), this->server->serverId, this->sessionId);
        this->upstream->RecvClientData(this->sessionId, payload, len - sizeof(packet_header));
    }
}

void Client::recvPakcetHeartbeat(const char* data, size_t len) { 
    this->LogAccess("| %15s | %ld | %ld | C=>G | heartbeat |", 
                this->remoteAddr.c_str(), this->server->serverId, this->sessionId);

    this->LogDebug("[client] recv a packet, type=heartbeat");
    if (this->status != client_status_working) {
        this->LogError("[client] recvPakcetHeartbeat failed, status=%d, error='status error, working expect'", this->status);
        return;
    }
    this->lastHeartbeatTime = this->server->Time();
}

void Client::RecvUpstreamData(Upstream* upstream, const char* data, size_t len) {
    static thread_local char buffer[65535];
    packet_header* header = (packet_header*)buffer;
    header->opcode = packet_type_data;
    memcpy(buffer + sizeof(packet_header), data, len);
    this->Send(buffer, sizeof(packet_header) + len);

    this->LogAccess("| %15s | %ld | %ld | %ld | G=>C | data |", 
                this->remoteAddr.c_str(), this->server->serverId, len, this->sessionId);
}

void Client::RecvUpstreamKick(Upstream* upstream, const char* data, size_t len) {
    static thread_local char buffer[65535];
    packet_header* header = (packet_header*)buffer;
    header->opcode = packet_type_kick;
    memcpy(buffer + sizeof(packet_header), data, len);
    this->Send(buffer, sizeof(packet_header) + len);
    this->DelayClose();

    this->LogAccess("| %15s | %ld | %ld | G=>C | kick |", 
                this->remoteAddr.c_str(), this->server->serverId, this->sessionId);
}

void Client::LogAccess(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if(this->location) {
        this->location->LogAccess(fmt, args);
    } else {
        this->server->LogAccess(fmt, args);
    }
    va_end(args);
}

void Client::LogError(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if(this->location) {
        this->location->LogError(fmt, args);
    } else {
        this->server->LogError(fmt, args);
    }
    va_end(args);
}

void Client::LogDebug(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    this->gate->LogDebug(fmt, args);
    va_end(args);
}

void Client::checkHeartbeat() {
    if (this->status != client_status_working) {
        return;
    }
    if (this->server->Time() - this->lastHeartbeatTime > this->server->config.heartbeat * 2) {
        this->LogError("[client] %s heartbeat timeout", this->remoteAddr.c_str());
        this->Close();
    }
    Json::Value response;
    this->replyJson(packet_type_heartbeat, response);
}

void Client::checkTimeout() {
    if (this->status != client_status_start) {
        return;
    }
    if (this->server->Time() - this->startTime > this->server->config.timeout) {
        this->LogError("[client] %s timeout", this->remoteAddr.c_str());
        this->Close();
    }
}

int Client::locationRemove(Location* location) {
    if (location != this->location) {
        return 0;
    }
    if (this->location == nullptr) {
        return 0;
    }
    Json::Value response;
    response["code"] = 0;
    this->replyJson(packet_type_down, response);
    this->DelayClose();
    this->location = nullptr;
    return 0;
}

int Client::upstreamRemove(Upstream* upstream) {
    if (this->upstream != upstream) {
        return 0;
    }
    if (this->upstream == nullptr) {
        return 0;
    }
    this->upstream = nullptr;
    Json::Value response;
    response["code"] = 0;
    this->replyJson(packet_type_down, response);
    this->DelayClose();
    return 0;
}



