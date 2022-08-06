#pragma once
#include "client.h"
#include "http-parser/http_parser.h"
#include <string>
#include <map>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>

class Gate;
class Server;
class Client;

class WebsocketClient : public IClientHandler {
public:
    WebsocketClient(Gate* gate, Server* server, Client* client);
    virtual ~WebsocketClient();
public:
    virtual int Unpack(const char* buffer, size_t len);
    virtual int Pack(const char* data, size_t len);
    int recvEncryptData(const char* data, size_t len);
    int recvData(const char* data, size_t len);
    void readDecryptData();
    int writeBioToSocket();
    void onHeaderValue(std::string& key, std::string& value);
    void onMessageComplete();
    int send(const char* data, size_t len);
    int sendResponse(const char* data, size_t len);
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
    SSL*                                ssl;
    BIO*                                read_bio;
    BIO*                                write_bio;
};

WebsocketClient* NewWebsocketClient(Gate* gate, Server* server, Client* client);
