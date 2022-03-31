#pragma once
#include "client.h"
class Gate;
class Client;
class Server;

class ConsoleClient : public IClientHandler {
public:
    ConsoleClient(Gate* gate, Server* server, Client* client);
    virtual ~ConsoleClient();
public:
    virtual int Unpack(const char* buffer, size_t len);
    virtual int Pack(const char* data, size_t len) {return 0;}
public:
    Gate*   gate;
    Client* client;
};

ConsoleClient* NewConsoleClient(Gate* gate, Server* server, Client* client);
