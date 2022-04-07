#include "client/console_client.h"
#include "client.h"
#include "errors.h"
#include "json/json.h"
#include "gate.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <cstdarg>

static unsigned char ctrl_c[5] = {0xff, 0xf4, 0xff, 0xfd, 0x06};

typedef int (ConsoleClient::*HandleFunc)(int argc, char** argv);

static std::map<std::string, HandleFunc> handleFuncDict {
    {"auth", &ConsoleClient::handleAuth},
    {"select", &ConsoleClient::handleSelect},
    {"help", &ConsoleClient::handleHelp},
    {"login", &ConsoleClient::handleLogin},
    {"reload", &ConsoleClient::handleReload},
    {"shutdown", &ConsoleClient::handleShutdown},
};

#define MAX_ARG_COUNT 10

enum packet_type {
    packet_type_handshake       = 1,
    packet_type_handshake_ack   = 2,
    packet_type_data            = 3,
    packet_type_heartbeat       = 4,
    packet_type_kick            = 5,
    packet_type_down            = 6,
    packet_type_maintain        = 7,
    packet_type_select          = 8,
};

struct packet_header {
    uint8_t opcode:4;
    uint8_t rsv:3;
    uint8_t fin:1;
    uint8_t reserve;
};

ConsoleClient* NewConsoleClient(Gate* gate, Server* server, Client* client) {
    ConsoleClient* self = new ConsoleClient(gate, server, client);
    return self;
}

ConsoleClient::ConsoleClient(Gate* gate, Server* server, Client* client) {
    this->gate = gate;
    this->client = client;
}

ConsoleClient::~ConsoleClient() {

}

void ConsoleClient::recvPakcetHandshake(const char* data, size_t len) {
    const char* payload = (char*)data + sizeof(packet_header);
    Json::Reader reader;
    Json::Value root;
    // reader将Json字符串解析到root，root将包含Json里所有子元素  
    if (!reader.parse(payload, payload + len - sizeof(packet_header), root)) {
        this->client->logError("[console_client] recvPakcetHandshake failed, error='json parse'");
        return;
    }
    if (root["code"] == 200) {
        thread_local static char buffer[1024];
        packet_header* header = (packet_header*)buffer;
        header->opcode = packet_type_handshake_ack;
        Json::Value payload;
        Json::FastWriter writer;
        std::string data = writer.write(payload);
        memcpy(buffer + sizeof(packet_header), data.c_str(), data.length());
        this->client->Recv(buffer, sizeof(packet_header) + data.length());
        this->replyf("OK\r\n");
    } else {
        this->replyf("%s\r\n", root["msg"].asString().c_str());
    }
}


int ConsoleClient::Pack(const char* data, size_t len) {
    packet_header* header = (packet_header*)data;
    switch(header->opcode) {
        case packet_type_handshake: {
            this->recvPakcetHandshake(data, len);
        }break;
    }
    return 0;
}

int ConsoleClient::Unpack(const char* buffer, size_t len) {
    if (len == sizeof(ctrl_c) && memcmp(buffer, ctrl_c, len) == 0) {
        return len;
    }
    static const char* delimited = "\r\n";
    const char* pos = strstr(buffer, delimited);
    if (pos == nullptr) {
        return 0;
    }
    if (pos == buffer) {
        return pos - buffer + 2;
    }
    *((char*)pos) = 0;
    char* argv[MAX_ARG_COUNT];
    int argc = 0;
    char* ptr = (char*)buffer;
    argv[argc++] = ptr;
    int searchBlank = 1;
    while(*ptr != 0) {
        if (searchBlank) {
            if (*ptr == ' ') {
                *ptr = 0;
                searchBlank = 0;
            }
        } else {
            if (*ptr != ' ') {
                searchBlank = 1;
                argv[argc++] = ptr;
                if(argc >= MAX_ARG_COUNT) {
                    break;
                }
            }
        }
        ptr++;
    }
    int err = this->handleCommand(argc, argv);
    switch(err) {
        case e_command_not_found:
            {
                this->replyf("%s: command not found...\n", argv[0]);
            }
            break;
        case e_invalid_args:
            {
                this->replyf("invalid args\n");
            }
            break;
    }
    return pos - buffer + 2;
}

void ConsoleClient::reply(const char* data, size_t len) {
    byte_array* frame = this->client->WillSend(len);
    if (nullptr == frame) {
        return;
    }
    char* buffer = frame->back();
    memcpy(buffer, data, len);
    frame->write(len);
    return;
}

void ConsoleClient::replyf(const char* fmt, ...) {
    thread_local static char buffer[1024];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if (len < 0) {
        this->client->logError("[console_client] replyf failed, error='buffer overflow'");
        return;
    }
    this->reply(buffer, len + 1);
}

int ConsoleClient::handleSelect(int argc, char** argv) {
    thread_local static char buffer[1024];
    if (argc < 2) {
        return e_invalid_args;
    }
    packet_header* header = (packet_header*)buffer;
    header->opcode = packet_type_select;
    memcpy(buffer + sizeof(packet_header), argv[1], strlen(argv[1]));
    this->client->Recv(buffer, sizeof(packet_header) + strlen(argv[1]));
    return 0;
}

int ConsoleClient::handleAuth(int argc, char** argv) {
    thread_local static char buffer[1024];
    if (argc < 2) {
        return e_invalid_args;
    }
    packet_header* header = (packet_header*)buffer;
    header->opcode = packet_type_handshake;
    Json::Value payload;
    payload["password"] = argv[1];
    Json::FastWriter writer;
    std::string data = writer.write(payload);
    memcpy(buffer + sizeof(packet_header), data.c_str(), data.length());
    this->client->Recv(buffer, sizeof(packet_header) + data.length());
    return 0;
}

int ConsoleClient::handleHelp(int argc, char** argv) {
    this->replyf("nothing\n");
    return 0;
}

int ConsoleClient::handleShutdown(int argc, char** argv) {
    int err = this->gate->shutdown();
    if (err) {
    } else {
        this->replyf("ok\r\n");
    }
    return 0;
}

int ConsoleClient::handleReload(int argc, char** argv) {
    int err = this->gate->reload();
    if (err) {
    } else {
        this->replyf("ok\r\n");
    }
    return 0;
}

int ConsoleClient::handleLogin(int argc, char** argv) {
    return 0;
}

int ConsoleClient::handleCommand(int argc, char** argv) {
    for (int i = 0; i < argc; i++) {
        printf("aa %s\n", argv[i]);
    }
    auto it = handleFuncDict.find(argv[0]);
    if (it == handleFuncDict.end()) {
        return e_command_not_found;
    }
    HandleFunc func = it->second;
    return (this->*func)(argc, argv);
}

