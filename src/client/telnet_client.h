#pragma once
#include "client.h"
class Gate;
class Client;
class Server;

class TelnetClient : public IClientHandler {
public:
    TelnetClient(Gate* gate, Server* server, Client* client);
    virtual ~TelnetClient();
public:
    virtual int Unpack(const char* buffer, size_t len);
    virtual int Pack(const char* data, size_t len) {return 0;}
public:
    Gate*   gate;
    Client* client;
};

TelnetClient* NewTelnetClient(Gate* gate, Server* server, Client* client);
