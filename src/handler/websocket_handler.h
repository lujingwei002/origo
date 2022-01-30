#pragma once
#include "client.h"
#include "http-parser/http_parser.h"
#include <string>
#include <map>

class Gate;
class Server;
class Client;

class WebsocketHandler : public IClientHandler {
public:
    WebsocketHandler(Gate* gate, Server* server, Client* client);
    virtual ~WebsocketHandler();
public:
    virtual int Unpack(const char* buffer, size_t len);
    virtual int Pack(const char* data, size_t len);
    void onHeaderValue(std::string& key, std::string& value);
    void onMessageComplete();
public:
    void recvTextFrame(const char* data, uint64_t len);
    void recvBinaryFrame(const char* data, uint64_t len);
    void recvCloseFrame();
    void recvPingFrame();
public:
    Gate*                               gate;
    Client*                             client;
    Server*                             server;
    http_parser                         parser;
    http_parser_settings                setting;
    std::string                         secWebsocketKey;
    bool                                wantUpgrade;
    std::string                         headerParsing;
    std::string                         path;
    std::string                         url;
    bool                                isUpgrade;
};

WebsocketHandler* NewWebsocketHandler(Gate* gate, Server* server, Client* client);
