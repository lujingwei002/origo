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
    virtual int Pack(const char* data, size_t len);
    int handleAuth(int argc, char** argv);
    int handleSelect(int argc, char** argv);
    int handleLogin(int argc, char** argv);
    int handleReload(int argc, char** argv);
    int handleShutdown(int argc, char** argv);
    int handleHelp(int argc, char** argv);
private:
    void recvPakcetHandshake(const char* data, size_t len);
    void reply(const char* data, size_t len);
    void replyf(const char* fmt, ...);
    int handleCommand(int argc, char** argv);
public:
    Gate*   gate;
    Client* client;
};

ConsoleClient* NewConsoleClient(Gate* gate, Server* server, Client* client);
