#include "handler/telnet_handler.h"
#include "client.h"
#include <cstdio>
#include <cstring>

TelnetHandler* NewTelnetHandler(Gate* gate, Client* client) {
    TelnetHandler* self = new TelnetHandler(gate, client);
    return self;
}

TelnetHandler::TelnetHandler(Gate* gate, Client* client) {
    this->gate = gate;
    this->client = client;
}

TelnetHandler::~TelnetHandler() {

}

int TelnetHandler::Unpack(const char* buffer, size_t len) {
    static const char* delimited = "\r\n";
    const char* pos = strstr(buffer, delimited);
    if (pos == nullptr) {
        return 0;
    }
    *((char*)pos) = 0;
    return pos - buffer + 2;
}
