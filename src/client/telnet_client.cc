#include "client/telnet_client.h"
#include "client.h"
#include <cstdio>
#include <cstring>

TelnetClient* NewTelnetClient(Gate* gate, Server* server, Client* client) {
    TelnetClient* self = new TelnetClient(gate, server, client);
    return self;
}

TelnetClient::TelnetClient(Gate* gate, Server* server, Client* client) {
    this->gate = gate;
    this->client = client;
}

TelnetClient::~TelnetClient() {

}

int TelnetClient::Unpack(const char* buffer, size_t len) {
    static const char* delimited = "\r\n";
    const char* pos = strstr(buffer, delimited);
    if (pos == nullptr) {
        return 0;
    }
    *((char*)pos) = 0;
    return pos - buffer + 2;
}
