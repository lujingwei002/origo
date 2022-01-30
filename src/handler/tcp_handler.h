#pragma once
#include "client.h"
class Gate;
class Client;

class TcpHandler : public IClientHandler {
public:
    TcpHandler(Gate* gate, Client* client);
    virtual ~TcpHandler();
public:
    virtual int Unpack(const char* buffer, size_t len);
    virtual int Pack(const char* data, size_t len);
public:
    Gate*   gate;
    Client* client;
};

TcpHandler* NewTcpHandler(Gate* gate, Client* client);
