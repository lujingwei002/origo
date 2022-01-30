#include "handler/tcp_handler.h"
#include "client.h"
#include <cstdio>
#include <cstring>

TcpHandler* NewTcpHandler(Gate* gate, Client* client) {
    TcpHandler* self = new TcpHandler(gate, client);
    return self;
}

TcpHandler::TcpHandler(Gate* gate, Client* client) {
    this->gate = gate;
    this->client = client;
}

TcpHandler::~TcpHandler() {

}

int TcpHandler::Unpack(const char* buffer, size_t len) {
    if (len < sizeof(uint16_t)){
        return 0;
    }
    uint16_t* lengths = (uint16_t*)buffer;
    uint16_t payload_len = lengths[0] << 8 || lengths[1];
    if (len < sizeof(uint16_t) + payload_len) {
        return 0;
    }
    this->client->Recv(buffer + sizeof(uint16_t), payload_len);
    return sizeof(uint16_t) + payload_len;
}

int TcpHandler::Pack(const char* data, size_t len) {
    uint16_t payload_len = len;
    byte_array* frame = this->client->WillSend(sizeof(uint16_t) + len);
    if (nullptr == frame) {
        return -1;
    }
    char* buffer = frame->data();
    buffer[0] = (payload_len >> 8) & 0xff;
    buffer[1] = (payload_len) & 0xff;
    memcpy(buffer + sizeof(uint16_t), data, payload_len);
    frame->write(sizeof(uint16_t) + payload_len);
    return 0;
}
