#include "upstream.h"
#include "upstream_group.h"
#include "upstream/tcp_upstream.h"
#include "upstream/redis_upstream.h"
#include "upstream/sock_upstream.h"
#include "gate.h"
#include "config.h"
#include "client.h"
#include "errors.h"
#include "byte_array.h"
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

/*

 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-------------------------------+-+-+-+-+-------+---------------+
|       Length                  |F|R|R|R| opcode|   Reserve     | 
|                               |I|S|S|S|  (4)  |     (8)       |
|                               |N|V|V|V|       |               |
|                               | |1|2|3|       |               |
+-------------------------------+-------------------------------+
|                     Session ID                                | 
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
:                     Session ID   continued                    :
+---------------------------------------------------------------+
:                     Payload Data continued                    :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
|                     Payload Data continued ...                |
+---------------------------------------------------------------+

*/

enum packet_type {
    packet_type_new             = 1,
    packet_type_close           = 2,
    packet_type_data            = 3,
    packet_type_heartbeat       = 4,
    packet_type_kick            = 5,
    packet_type_down            = 6,
};

#pragma pack(1)
struct packet_header {
    uint16_t    length;
    uint8_t     opcode:4;
    uint8_t     rsv:3;
    uint8_t     fin:1;
    uint8_t     reserve;
    uint64_t    sessionId;
};
#pragma pack()

Upstream* NewUpstream(Gate* gate, UpstreamGroup* group, UpstreamConfig& config) {
    Upstream* self = new Upstream(gate, group, config);
    return self;
}

Upstream::Upstream(Gate* gate, UpstreamGroup* group, UpstreamConfig& config) {
    this->gate = gate;
    this->group = group;
    this->config = config;
    this->status = upstream_status_none;
    this->driver = nullptr;
}

Upstream::~Upstream() {
    if (this->driver) {
        delete this->driver;
        this->driver = nullptr;
    }
}

void Upstream::Close() {
    if (this->driver == nullptr) {
        return;
    }
    this->driver->Close();
}

void Upstream::DelayClose() {
    if (this->driver == nullptr) {
        return;
    }
    this->driver->DelayClose();
}

int Upstream::Unpack(const char* data, size_t len) {
    if (len < sizeof(uint16_t)) {
        return 0;
    }
    uint16_t length = *((uint16_t*)data);
    length = ntohs(length);
    if (len < sizeof(uint16_t) + length) {
        return 0;
    }
    packet_header header;
    memcpy(&header, data, sizeof(packet_header));
    header.length = length;
    header.sessionId = ((uint64_t)ntohl(header.sessionId & 0xffffffff)) << 32 | ntohl((header.sessionId >> 32) & 0xffffffff);

    uint64_t sessionId = header.sessionId;
    switch(header.opcode) {
        case packet_type_kick:
            {
                this->gate->recvUpstreamKick(this, sessionId, data + sizeof(packet_header), len - sizeof(packet_header));
            }break;
        case packet_type_data:
            {
                this->gate->recvUpstreamData(this, sessionId, data + sizeof(packet_header), len - sizeof(packet_header));
            }break;
        default: {
            this->logError("[upstream] Unpack fail, opcode=%d, error='opcode error'", header.opcode);
        }
    }
    return sizeof(uint16_t) + header.length;
}

void Upstream::TryReconnect() {
    if(this->driver == nullptr) {
        return;
    }
    this->driver->TryReconnect();
}

void Upstream::TryHeartbeat() {
    if (this->status != upstream_status_connect) {
        return;
    }
    packet_header header;
    header.opcode = packet_type_heartbeat;
    header.length = htons(sizeof(packet_header) + 0 - sizeof(uint16_t));
    header.sessionId = 0;
    this->driver->RecvClientData((const char*)&header, sizeof(packet_header), nullptr, 0);
}

int Upstream::start() {
    if (this->status != upstream_status_none) {
        this->logError("[upstream] start fail, status=%d, error='status error'", this->status);
        return e_status;
    }
    UpstreamDriver* driver = nullptr;
    if (this->config.type == "tcp") {
        driver = NewTcpUpstream(this);
    } else if (this->config.type == "redis") {
        driver = NewRedisUpstream(this);
    } else if (this->config.type == "sock") {
        driver = NewSockUpstream(this);
    } else {
        return e_upstream_invalid; 
    }
    if (nullptr == driver) {
        return e_out_of_menory;
    }
    int err = driver->start();;
    if (err) {
        delete driver;
        return err;
    }
    this->driver = driver;
    return 0;
}

void Upstream::RecvClientNew(uint64_t sessionId) {
    this->logDebug("[upstream] RecvClientNew, sessionId=%ld", sessionId);
    if (nullptr == this->driver) {
        return;
    }
    packet_header header;
    header.opcode = packet_type_new;
    header.reserve = 0;
    header.rsv = 0;
    header.fin = 0;
    header.length = htons(sizeof(packet_header) - sizeof(uint16_t));
    header.sessionId = ((uint64_t)ntohl(sessionId & 0xffffffff)) << 32 | ntohl((sessionId >> 32) & 0xffffffff);;
    this->driver->RecvClientData((const char*)&header, sizeof(packet_header), nullptr, 0);
}

void Upstream::RecvClientClose(uint64_t sessionId) {
    this->logDebug("[upstream] RecvClientClose, sessionId=%ld", sessionId);
    if (nullptr == this->driver) {
        return;
    }
    packet_header header;
    header.opcode = packet_type_close;
    header.length = htons(sizeof(packet_header) - sizeof(uint16_t));
    header.sessionId = ((uint64_t)ntohl(sessionId & 0xffffffff)) << 32 | ntohl((sessionId >> 32) & 0xffffffff);;
    this->driver->RecvClientData((const char*)&header, sizeof(packet_header), nullptr, 0);
}

void Upstream::RecvClientData(uint64_t sessionId, const char* data, size_t len) { 
    this->logDebug("[upstream] RecvClientData, sessionId=%ld, len=%ld", sessionId, len);
    if (nullptr == this->driver) {
        return;
    }
    packet_header header;
    header.opcode = packet_type_data;
    header.length = htons(sizeof(packet_header) + len - sizeof(uint16_t));
    header.sessionId = ((uint64_t)ntohl(sessionId & 0xffffffff)) << 32 | ntohl((sessionId >> 32) & 0xffffffff);;
    this->driver->RecvClientData((const char*)&header, sizeof(packet_header), data, len);
}

void Upstream::logDebug(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    this->gate->logDebug(fmt, args);
    va_end(args);
}

void Upstream::logError(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    this->gate->logError(fmt, args);
    va_end(args);
}

void Upstream::groupShutdown() {
    packet_header header;
    header.opcode = packet_type_down;
    header.length = htons(sizeof(packet_header) - sizeof(uint16_t));
    header.sessionId = 0;
    this->driver->RecvClientData((const char*)&header, sizeof(packet_header), nullptr, 0);
    this->DelayClose();
}
void Upstream::shutdown() {
    packet_header header;
    header.opcode = packet_type_down;
    header.length = htons(sizeof(packet_header) - sizeof(uint16_t));
    header.sessionId = 0;
    this->driver->RecvClientData((const char*)&header, sizeof(packet_header), nullptr, 0);
    this->DelayClose();
}

int Upstream::reload(UpstreamConfig& config) {
    this->config = config;
    return 0;
}
