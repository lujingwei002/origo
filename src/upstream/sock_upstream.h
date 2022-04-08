#pragma once 

#include "config.h"
#include "upstream.h"
#include "byte_array.h"
#include <netinet/in.h>
#include <sys/un.h>

class Upstream;

class SockUpstream : public UpstreamDriver {
public:
    SockUpstream(Upstream* upstream);
    virtual ~SockUpstream();
public:
    virtual int start();
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
    struct sockaddr_un      addr;
    byte_array*             recvBuffer;
    std::deque<byte_array*> sendDeque;
    int                     sockfd;
};

SockUpstream* NewSockUpstream(Upstream* upstream);
