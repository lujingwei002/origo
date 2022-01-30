#pragma once 

#include "config.h"
#include "upstream.h"
#include "byte_array.h"
#include <netinet/in.h>

class Upstream;

class TcpUpstream : public UpstreamDriver {
public:
    TcpUpstream(Upstream* upstream);
    virtual ~TcpUpstream();
public:
    virtual int Start();
    virtual void TryReconnect();
    virtual void Close();
    virtual void DelayClose();
    virtual void RecvClientData(const char* header, size_t headerLen, const char* payload, size_t len);
public:
    void onConnectError();
    void onConnectSucc();
    void onRead();
    void onClose();
    void onWrite();
    bool checkConnecting();
    byte_array* WillSend(size_t len);
public:
    Upstream*               upstream;
    struct sockaddr_in      addr;
    byte_array*             recvBuffer;
    std::deque<byte_array*> sendDeque;
    int                     sockfd;
};

TcpUpstream* NewTcpUpstream(Upstream* upstream);
