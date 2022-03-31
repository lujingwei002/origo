#include "client/console_client.h"
#include "client.h"
#include <cstdio>
#include <cstring>

ConsoleClient* NewConsoleClient(Gate* gate, Server* server, Client* client) {
    ConsoleClient* self = new ConsoleClient(gate, server, client);
    return self;
}

ConsoleClient::ConsoleClient(Gate* gate, Server* server, Client* client) {
    this->gate = gate;
    this->client = client;
}

ConsoleClient::~ConsoleClient() {

}

int ConsoleClient::Unpack(const char* buffer, size_t len) {
    static const char* delimited = "\r\n";
    const char* pos = strstr(buffer, delimited);
    if (pos == nullptr) {
        return 0;
    }
    *((char*)pos) = 0;
    printf("ffffffff %s\n", buffer);
    return pos - buffer + 2;
}
