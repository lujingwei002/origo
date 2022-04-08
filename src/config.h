#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <map>

class Arguments {
public:
    Arguments() {
        this->help = false;
        this->testConfigureFile = false;
        this->daemon = false;
    }
public:
    std::string configureFilePath;
    bool        testConfigureFile;
    bool        help;
    std::string signal;
    bool        daemon;
};

extern Arguments arguments;

class UpstreamConfig {
public:
    UpstreamConfig();
public:
    void DebugString(std::stringstream& buffer, int level);
public:
    std::string     name;
    std::string     type;
    std::string     addr;
    std::string     host;
    std::string     user;
    std::string     password;
    std::string     query;
    std::string     channel;
    uint16_t        port;
    int             weight;
    std::map<std::string, std::string> args;
};

class UpstreamGroupConfig {
public:
    UpstreamGroupConfig();
public:
    void DebugString(std::stringstream& buffer, int level);
public:
    std::string                             name;
    int16_t                                 heartbeat;
    int16_t                                 reconnect;
    uint16_t                                sendBufferSize;
    uint16_t                                recvBufferSize;
    std::map<std::string, UpstreamConfig>   upstreamDict;
};

class LocationConfig {
public:
    LocationConfig();
public:
    void DebugString(std::stringstream& buffer, int level);
public:
    std::string     path;
    std::string     proxyPass;
    std::string     accessLog;
    std::string     errorLog;
};

class ServerConfig {
public:
    ServerConfig();
public:
    void DebugString(std::stringstream& buffer, int level);
public:

    std::string                             name;
    std::string                             requirepass;
    std::string                             listen;
    std::string                             net;
    int16_t                                 port;
    std::string                             tslCertificate;
    int16_t                                 heartbeat;
    int16_t                                 timeout;
    bool                                    handshake;
    std::string                             accessLog;
    std::string                             errorLog;
    std::map<std::string, LocationConfig>   locationDict;
    int                                     maxConnPerSec;
    uint16_t                                sendBufferSize;
    uint16_t                                recvBufferSize;
    std::string                             maintain;
};

class Config {
public:
    Config();
    ~Config();
public:
    std::string DebugString();
public:
    int Parse(const char* path);
    bool ReadLine(std::string& line);
    int selfBegin(Config& self);
    int serverBegin(ServerConfig& self, std::vector<std::string>& _args);
    int upstreamBegin(UpstreamGroupConfig& self, std::vector<std::string>& _args);
    int locationBegin(LocationConfig& locationConfig, std::vector<std::string>& _args);
    int readArgs(std::vector<std::string>& args);
public:
    size_t                                          linePtr;
    std::vector<std::string>                        lineArr;
    int16_t                                         worker;
    bool                                            daemon;
    std::string                                     curLine;
    std::string                                     accessLog;
    std::string                                     errorLog;
    std::string                                     debugLog;
    std::string                                     pid;
    std::map<std::string, ServerConfig>             serverDict;
    std::map<std::string, UpstreamGroupConfig>      upstreamGroupDict;
    std::string                                     maintain;
};

Config* NewConfig(); 



