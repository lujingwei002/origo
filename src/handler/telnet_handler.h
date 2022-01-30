#pragma once
#include "client.h"
class Gate;
class Client;

class TelnetHandler : public IClientHandler {
public:
    TelnetHandler(Gate* gate, Client* client);
    virtual ~TelnetHandler();
public:
    virtual int Unpack(const char* buffer, size_t len);
    virtual int Pack(const char* data, size_t len) {return 0;}
public:
    Gate*   gate;
    Client* client;
};

TelnetHandler* NewTelnetHandler(Gate* gate, Client* client);
