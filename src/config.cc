#include "config.h"
#include "errors.h"
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <iomanip>

Arguments arguments;

#define READ_BEGIN()\
while(true){\
    std::vector<std::string> _args;\
    int err = this->readArgs(_args);\
    if (err == e_eof){\
        return 0;\
    } else if (err) { \
        return err;\
    }\
    if (_args.size() <= 0) {\
    }


#define READ_END() \
    else if(_args[0] == "}"){\
        return 0;\
    } else {\
    }\
}\
return 0;


#define READ_INT(k1, k2) \
    else if (_args[0] == k1) { \
      if (_args.size() < 2) { \
          throw exception(this->curLine+" expected integer");\
          return e_config_syntax;\
      }\
      self.k2 = atoi(_args[1].c_str());\
    } 

#define READ_STRING(k1, k2) \
    else if (_args[0] == k1) { \
      if (_args.size() != 2) { \
          throw exception(this->curLine+" expected string");\
          return e_config_syntax;\
      }\
      self.k2 = _args[1];\
    } 

#define READ_BOOL(k1, k2) \
    else if (_args[0] == k1) { \
      if (_args.size() < 2) { \
          throw exception(this->curLine+" expected true or false");\
          return e_config_syntax;\
      }\
      if (_args[1] == "true") {\
          self.k2 = true;\
      } else if(_args[1] == "false") {\
          self.k2 = false;\
      } else {\
          throw exception(this->curLine+" expected true or false");\
          return e_config_syntax;\
      }\
    } 

Config* NewConfig() {
    Config* self = new Config();
    return self;
} 

ServerConfig::ServerConfig() {
    this->handshake = false;
    this->maxConnPerSec = 100;
    this->sendBufferSize = 65535;
    this->recvBufferSize = 65535;
}

UpstreamGroupConfig::UpstreamGroupConfig() {
    this->heartbeat = 60;
    this->reconnect = 5;
    this->sendBufferSize = 65535;
    this->recvBufferSize = 65535;
}

UpstreamConfig::UpstreamConfig() {
    this->port = 0;
}

LocationConfig::LocationConfig() {
    
}

Config::Config() {
    this->linePtr = 0;
    this->worker = 1;
}

Config::~Config() {
}

int Config::Parse(const char* filePath) {
    std::ifstream file(filePath);
    if(!file){
        throw exception(std::string("Can't open file ")+filePath);
    }
    std::string line;
    while(std::getline(file, line)) {
        this->lineArr.push_back(line);
    } 
    int err = this->selfBegin(*this);
    return err;
}

bool Config::ReadLine(std::string& line) {
    if (this->linePtr >= this->lineArr.size()) {
        return false;
    }
    line = this->lineArr[this->linePtr];
    this->linePtr++;
    this->curLine = line;
    return true;
}

int Config::readArgs(std::vector<std::string>& args) {
    std::string line;
    if(!this->ReadLine(line)) {
        return e_eof;
    }
    args.clear();
    size_t last = 0;
    int mode = 1;// 0:单词结束 1:单词开始 2:字符串结束 :3:注释
    for (size_t i = 0; i < line.length(); i++) {
        char c = line[i];
        if (mode == 0) {
            if (c == ';') {
                mode = 3;
                args.push_back(line.substr(last, i - last));
                last = i + 1;
            } else if(std::isspace(c)) {
                args.push_back(line.substr(last, i - last));
                mode = 1;
            }
        } else if (mode == 1) {
            if (c == ';') {
                mode = 3;
                last = i + 1;
            } else if (c == '"') {
                mode = 2;
                last = i + 1;
            } else if(!std::isspace(c)) {
                mode = 0;
                last = i;
            }
        } else if (mode == 2) {
            if (c == '"') {
                mode = 1;
                args.push_back(line.substr(last, i - last));
                last = i + 1;
            }
        } else if (mode == 3) {
        }
    }
    if (mode == 0 && line.length() > 0) {
        args.push_back(line.substr(last, line.length() - last));
    }
    return 0;
}

int Config::selfBegin(Config& self) {
    READ_BEGIN()
    READ_BOOL("daemon", daemon)
    READ_INT("worker", worker)
    READ_STRING("maintain", maintain)
    READ_STRING("pid", pid)
    READ_STRING("access_log", accessLog)
    READ_STRING("error_log", errorLog)
    READ_STRING("debug_log", debugLog)
    else if (_args.back() == "{" && _args[0] == "server") {
        ServerConfig serverConfig;
        int err = this->serverBegin(serverConfig, _args);
        if (err) {
            return err;
        }
        this->serverDict[serverConfig.name] = serverConfig;
    } else if(_args.back() == "{" && _args[0] == "upstream") {
        UpstreamGroupConfig upstreamGroupConfig;
        int err = this->upstreamBegin(upstreamGroupConfig, _args);
        if (err) {
            return err;
        }
        this->upstreamGroupDict[upstreamGroupConfig.name] = upstreamGroupConfig;
    }
    READ_END()
    return 0;
}

int Config::serverBegin(ServerConfig& self, std::vector<std::string>& _args) {
    if (_args.size() != 3) {
        throw exception(this->curLine+" syntax error");
        return e_config_syntax;
    }
    self.name = _args[1];

    READ_BEGIN()
    READ_STRING("maintain", maintain)
    READ_STRING("requirepass", requirepass)
    READ_STRING("access_log", accessLog)
    READ_STRING("error_log", errorLog)
    READ_INT("heartbeat", heartbeat)
    READ_INT("max_conn_per_sec", maxConnPerSec)
    READ_INT("send_buffer_size", sendBufferSize)
    READ_INT("recv_buffer_size", recvBufferSize)
    READ_INT("timeout", timeout)
    READ_BOOL("handshake", handshake)
    else if(_args[0] == "location") {
        LocationConfig locationConfig;
        int err = this->locationBegin(locationConfig, _args);
        if (err) {
            return err;
        }
        self.locationDict[locationConfig.path] = locationConfig;
    } else if(_args[0] == "listen") {
        if (_args.size() <= 2){
            self.net = "ws";
            self.listen = _args[1];
        } else if(_args.size() <= 3) {
            self.net = _args[1];
            self.listen = _args[2];
        } else {
            throw exception(this->curLine+" syntax error");
            return e_config_syntax;
        }
        self.port = atol(self.listen.c_str());
        if (self.port == 0) {
            throw exception(this->curLine+" syntax error");
            return e_config_syntax;
        }     
    }
    READ_END()
}
int Config::locationBegin(LocationConfig& self, std::vector<std::string>& args) {
    if (args.size() != 3) {
        throw exception(this->curLine+" syntax error");
        return e_config_syntax;
    }
    self.path = args[1];
    READ_BEGIN()
    READ_STRING("access_log", accessLog)
    READ_STRING("error_log", errorLog)
    READ_STRING("proxy_pass", proxyPass)
    READ_END()
}

int Config::upstreamBegin(UpstreamGroupConfig& self, std::vector<std::string>& _args) {
    if (_args.size() != 3) {
        throw exception(this->curLine+" syntax error");
        return e_config_syntax;
    }
    self.name = _args[1];
    READ_BEGIN()
    READ_INT("heartbeat", heartbeat)
    READ_INT("reconnect", reconnect)
    READ_STRING("password", password)
    READ_INT("send_buffer_size", sendBufferSize)
    READ_INT("recv_buffer_size", recvBufferSize)
    else if(_args[0] == "tcp") {
        if (_args.size() < 2) {
            throw exception(this->curLine+" syntax error");
            return e_config_syntax;
        }
        UpstreamConfig upstreamConf;
        upstreamConf.type = _args[0];
        upstreamConf.addr = _args[1];
        upstreamConf.name = upstreamConf.type + "://" + upstreamConf.addr;
        if (upstreamConf.addr.find(":") == std::string::npos) {
            throw exception(this->curLine+" syntax error");
            return e_config_syntax;
        }
        upstreamConf.host = upstreamConf.addr.substr(0, upstreamConf.addr.find(":"));
        upstreamConf.port = atoi(upstreamConf.addr.substr(upstreamConf.addr.find(":") + 1).c_str());
        if(upstreamConf.port == 0) {
            throw exception(this->curLine+" syntax error");
            return e_config_syntax;
        }
        if (_args.size() > 2 && _args[2].find("weight=") == 0) {
            upstreamConf.weight = atoi(_args[2].substr(7).c_str());
        }
        self.upstreamDict[upstreamConf.name] = upstreamConf;    
    } else if(_args[0] == "redis") {
        if (_args.size() < 2) {
            throw exception(this->curLine+" syntax error");
            return e_config_syntax;
        }
        UpstreamConfig upstreamConf;
        upstreamConf.type = _args[0];
        upstreamConf.addr = _args[1];
        upstreamConf.name = upstreamConf.type + "://" + upstreamConf.addr;
        if (upstreamConf.addr.find(":") == std::string::npos) {
            throw exception(this->curLine+" syntax error");
            return e_config_syntax;
        }
        upstreamConf.host = upstreamConf.addr.substr(0, upstreamConf.addr.find(":"));
        upstreamConf.port = atoi(upstreamConf.addr.substr(upstreamConf.addr.find(":") + 1).c_str());
        if(upstreamConf.port == 0) {
            throw exception(this->curLine+" syntax error");
            return e_config_syntax;
        }
        if (_args.size() > 2 && _args[2].find("weight=") == 0) {
            upstreamConf.weight = atoi(_args[2].substr(7).c_str());
        }
        self.upstreamDict[upstreamConf.name] = upstreamConf;

    }
    READ_END()
}

std::string Config::DebugString() {
    std::stringstream buffer;
    buffer << "############################################" << std::endl;
    int level = 0;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "worker:" << this->worker << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "pid:" << this->pid << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "access_log:" << this->accessLog << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "error_log:" << this->errorLog << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "maintain:" << this->maintain << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "daemon:" << std::boolalpha << this->daemon << std::endl;
    for (auto& it : this->serverDict) {
        it.second.DebugString(buffer, level + 1);
    } 
    for (auto& it : this->upstreamGroupDict) {
        it.second.DebugString(buffer, level + 1);
    } 
    buffer << "############################################" << std::endl;
    return buffer.str();
}


void ServerConfig::DebugString(std::stringstream& buffer, int level) {
    buffer << std::setfill(' ') << std::setw((level-1)*4) << " " << "server:" << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "name:" << this->name << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "listen:" << this->listen << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "net:" << this->net << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "maintain:" << this->maintain << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "port:" << this->port << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "max_conn_per_sec:" << this->maxConnPerSec << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "recv_buffer_size:" << this->recvBufferSize << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "send_buffer_size:" << this->sendBufferSize << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "requirepass:" << this->requirepass << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "tslCertificate:" << this->tslCertificate<< std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "heartbeat:" << this->heartbeat << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "timeout:" << this->timeout << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "handshake:" << std::boolalpha << this->handshake<< std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "access_log:" << this->accessLog << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "error_log:" << this->errorLog << std::endl;
    for (auto& it : this->locationDict) {
        it.second.DebugString(buffer, level + 1);
    } 
}

void LocationConfig::DebugString(std::stringstream& buffer, int level) {
    buffer << std::setfill(' ') << std::setw((level-1)*4) << " " << "location:" << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "path:" << this->path << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "proxy_pass:" << this->proxyPass << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "access_log:" << this->accessLog << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "error_log:" << this->errorLog << std::endl;
}

void UpstreamGroupConfig::DebugString(std::stringstream& buffer, int level) {
    buffer << std::setfill(' ') << std::setw((level-1)*4) << " " << "upstream " << this->name << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "heartbeat:" << this->heartbeat << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "reconnect:" << this->reconnect << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "password:" << this->password << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "recv_buffer_size:" << this->recvBufferSize << std::endl;
    buffer << std::setfill(' ') << std::setw(level*4) << " " << "send_buffer_size:" << this->sendBufferSize << std::endl;
    for (auto& it : this->upstreamDict) {
        it.second.DebugString(buffer, level);
    } 
}

void UpstreamConfig::DebugString(std::stringstream& buffer, int level) {
    buffer << std::setfill(' ') << std::setw(level*4) << " " << this->type << " " << this->addr << " " << "weight=" << this->weight << std::endl;
}
