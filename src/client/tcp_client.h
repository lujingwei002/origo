#pragma once
#include "client.h"
class Gate;
class Client;

class TcpClient : public IClientHandler {
public:
    TcpClient(Gate* gate, Client* client);
    virtual ~TcpClient();
public:
    virtual int Unpack(const char* buffer, size_t len);
    virtual int Pack(const char* data, size_t len);
public:
    Gate*   gate;
    Client* client;
};

TcpClient* NewTcpClient(Gate* gate, Client* client);
