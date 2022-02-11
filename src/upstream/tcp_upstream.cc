#include "upstream/tcp_upstream.h"
#include "gate.h"
#include "upstream_group.h"
#include "errors.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>

static void on_connect_err(struct aeEventLoop *eventLoop, int sockfd, void *args, int event) {
    TcpUpstream* upstream = (TcpUpstream*)args;
    upstream->onConnectSucc();
}

static void on_connect_succ(struct aeEventLoop *eventLoop, int sockfd, void *args, int event){
    TcpUpstream* upstream = (TcpUpstream*)args;
    upstream->onConnectSucc();
}

static void on_read(struct aeEventLoop *eventLoop, int sockfd, void *args, int event) {
    TcpUpstream* upstream = (TcpUpstream*)args;
    upstream->onRead();
}

static void on_write(struct aeEventLoop *eventLoop, int sockfd, void *args, int event) {
    TcpUpstream* upstream = (TcpUpstream*)args;
    upstream->onWrite();
}

TcpUpstream* NewTcpUpstream(Upstream* upstream) {
    TcpUpstream* self = new TcpUpstream(upstream);
    return self;
}

TcpUpstream::TcpUpstream(Upstream* upstream) {
    this->upstream = upstream;
    this->sockfd = -1;
    this->recvBuffer = nullptr;
}

TcpUpstream::~TcpUpstream() {
    Upstream* upstream = this->upstream;
    upstream->LogDebug("[tcp_upstream] ~TcpUpstream");
    if(this->recvBuffer) {
        upstream->group->FreeBuffer(this->recvBuffer);
        this->recvBuffer = nullptr;
    }
    while(this->sendDeque.size() > 0) {
        byte_array* b = this->sendDeque.back();
        this->sendDeque.pop_back();
        b->reset();
        upstream->group->FreeBuffer(b);
    }
    if(this->sockfd != -1) {
        aeDeleteFileEvent(upstream->gate->loop, this->sockfd, AE_READABLE | AE_WRITABLE);
        close(this->sockfd);
        this->sockfd = -1;
    }
}

void TcpUpstream::DelayClose() {
    Upstream* upstream = this->upstream;
    if (upstream->status == upstream_status_delayclose || upstream->status == upstream_status_closing || upstream->status == upstream_status_closed) {
        return;
    }
    upstream->status = upstream_status_delayclose;
    aeDeleteFileEvent(upstream->gate->loop, this->sockfd, AE_READABLE);
    //只关闭读
    shutdown(this->sockfd,SHUT_RD); 
}

void TcpUpstream::Close() {
    Upstream* upstream = this->upstream;
    if (upstream->status == upstream_status_closing || upstream->status == upstream_status_closed) {
        return;
    }
    upstream->status = upstream_status_closing;
    shutdown(this->sockfd,SHUT_RDWR); 
}

void TcpUpstream::onClose() {
    Upstream* upstream = this->upstream;
    upstream->LogDebug("[tcp_upstream] onClose");
    if (upstream->status == upstream_status_delayclose) {
        //关闭写
        shutdown(this->sockfd, SHUT_RDWR);
    } else if (upstream->status == upstream_status_closing) {
        
    }
    upstream->group->onUpstreamClose(upstream);
    upstream->status = upstream_status_closed;

    // 由于要重连，这里就释放掉资源先
     aeDeleteFileEvent(upstream->gate->loop, this->sockfd, AE_READABLE | AE_WRITABLE);
}

void TcpUpstream::onRead() {
    Upstream* upstream = this->upstream;
    if (this->sockfd < 0) {
        upstream->LogError("[tcp_upstream] onRead failed, error='sockfd invalid'");
        return;
    }
    if (upstream->status != upstream_status_connect) {
        upstream->LogError("[tcp_upstream] onRead failed, error='status invalid'");
        return;
    }
    for (;;) {
        int ir = recv(this->sockfd, (void*)this->recvBuffer->back(), this->recvBuffer->capacity(), 0); 
        int last_errno = errno;
        upstream->LogDebug("[tcp_upstream] onRead, recv %d", ir);
        if (ir == 0 || (ir == -1 && last_errno != EAGAIN)) {
            //close
            this->onClose();
            return;
        }
        if (ir == -1 && last_errno == EAGAIN) {
            upstream->LogDebug("[tcp_upstream] onRead, not more data, wait");
            //wait data
            break;
        }
        this->recvBuffer->write(ir);
        for(;;) {
            int r = upstream->Unpack(this->recvBuffer->front(), this->recvBuffer->length());
            if (r > 0) {
                this->recvBuffer->read(r);
            } else if(r == 0) {
                //more data
                if (this->recvBuffer->front() == this->recvBuffer->data() && this->recvBuffer->back() == this->recvBuffer->end()) {
                    upstream->LogError("[client] onRead failed, error='buffer not enough'");
                    this->Close();
                    return;
                } else {
                    this->recvBuffer->truncate();
                    break;
                }
            }
        }

    }
}

void TcpUpstream::onWrite() {
    Upstream* upstream = this->upstream;
    //this->upstream->LogDebug("[tcp_upstream] onWrite");
    for(;;) {
        if (this->sendDeque.size() <= 0) {
            if (upstream->status == upstream_status_delayclose) {
                this->onClose();
                break;
            } else {
                aeDeleteFileEvent(upstream->gate->loop, sockfd, AE_WRITABLE);
                break;
            }
        }
        byte_array* b = this->sendDeque.back();
        if (nullptr == b) {
            if (upstream->status == upstream_status_delayclose) {
                this->onClose();
                break;
            } else {
                break;
            }
        }
        if (b->length() <= 0) {
            this->sendDeque.pop_back();
            aeDeleteFileEvent(upstream->gate->loop, sockfd, AE_WRITABLE);
            break;
        }
        //for(size_t i = 0; i < b->length(); i++) {
            //printf("%d ", (unsigned char)(b->data()[i]));
        //}
        //printf("\n");
        int ir = send(this->sockfd, b->front(), b->length(), 0);
        upstream->LogDebug("[tcp_upstream] onWrite, len=%ld, result=%d", b->length(), ir);
        if (ir > 0) {
            b->read(ir);
            if (b->length() <= 0) {
                this->sendDeque.pop_back();
                upstream->group->FreeBuffer(b);
            }
        } else if (ir == -1 && errno == EAGAIN) {
            break;
        } else if (ir == -1) {
            //this->Close();
            break;
        }
    }
}

void TcpUpstream::onConnectError() {
    Upstream* upstream = this->upstream;
    upstream->LogDebug("[tcp_upstream] onConnectError");
}

void TcpUpstream::onConnectSucc() {
    Upstream* upstream = this->upstream;
    aeDeleteFileEvent(upstream->gate->loop, sockfd, AE_READABLE|AE_WRITABLE);
    if (this->checkConnecting()) {
        upstream->LogDebug("[tcp_upstream] onConnectSucc");
        upstream->status = upstream_status_connect;
        aeCreateFileEvent(upstream->gate->loop, sockfd, AE_READABLE, on_read, (void*)this);
        aeCreateFileEvent(upstream->gate->loop, sockfd, AE_WRITABLE, on_write, (void*)this);
    } else {
        upstream->LogError("[tcp_upstream] onConnectError");
        upstream->status = upstream_status_connect_err;
        close(this->sockfd);
        this->sockfd = -1;
    }
}

bool TcpUpstream::checkConnecting() {
    Upstream* upstream = this->upstream;
    if (this->sockfd < 0) {
        return false;
    }
    if (upstream->status != upstream_status_connecting) {
        return false;
    }
    int err = connect(sockfd, (struct sockaddr *)&this->addr, sizeof(this->addr));
    if (err == 0) {
        return true;
    } else if (err && errno == EISCONN) {
        return true;
    }
    return false;
}

int TcpUpstream::Start() {
    Upstream* upstream = this->upstream;
    if (upstream->status != upstream_status_none) {
        upstream->LogError("[tcp_upstream] Start fail, status=%d, error='status error'", upstream->status);
        return e_status;
    }
    upstream->LogError("[tcp_upstream] Start, host=%s, port=%d, weight=%d", upstream->config.host.c_str(), upstream->config.port, upstream->config.weight);
    byte_array* recvBuffer = upstream->group->AllocBuffer(upstream->group->config.recvBufferSize);
    if (recvBuffer == nullptr) {
        upstream->LogError("[tcp_upstream] Start fail, error='alloc recv buffer'");
        return e_out_of_menory;
    }
    this->recvBuffer = recvBuffer;
    memset((void*)&this->addr, 0, sizeof(this->addr));
    this->addr.sin_family = AF_INET;
    const char* ip = upstream->config.host.c_str();
    if(inet_addr(ip) != (in_addr_t)-1) {
        this->addr.sin_addr.s_addr = inet_addr(ip);   
    } else {
        struct hostent *hostent;
        hostent = gethostbyname(ip);
        if(hostent->h_addr_list[0] == NULL) {
            upstream->LogError("[tcp_upstream] Start fail, ip=%s, error='gethostbyname'", ip);
            return e_socket;
        }
        memcpy(&this->addr.sin_addr, (struct in_addr *)hostent->h_addr_list[0], sizeof(struct in_addr));
    }
    this->addr.sin_port = htons(upstream->config.port);        
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        upstream->LogError("[tcp_upstream] Start failed, error='create socket'");
        return e_socket;
    }
    // non block
    int err = fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK);
    if (err < 0) {
        upstream->LogError("[tcp_upstream] Start failed, error='fcntl'");
        return e_socket;
    }

    // create event
    aeCreateFileEvent(upstream->gate->loop, sockfd, AE_READABLE, on_connect_err, (void*)this);
    aeCreateFileEvent(upstream->gate->loop, sockfd, AE_WRITABLE, on_connect_succ, (void*)this);

    err = connect(sockfd, (struct sockaddr *)&this->addr, sizeof(this->addr));
    if(err < 0 && errno != EINPROGRESS) {
        upstream->LogError("[tcp_upstream] Start fail, errno=%d, error='%s'", errno, strerror(errno));
        aeDeleteFileEvent(upstream->gate->loop, sockfd, AE_READABLE|AE_WRITABLE);
        close(sockfd);
        return e_socket;
    }
    upstream->status = upstream_status_connecting;
    this->sockfd = sockfd;
    return 0;
}

void TcpUpstream::TryReconnect() {
    Upstream* upstream = this->upstream;
    if (upstream->status != upstream_status_connect_err && upstream->status != upstream_status_closed) {
        return;
    }
    //释放之前的资源
    if(this->recvBuffer) {
        this->recvBuffer->reset();
    }
    while(this->sendDeque.size() > 0) {
        byte_array* b = this->sendDeque.back();
        this->sendDeque.pop_back();
        b->reset();
        upstream->group->FreeBuffer(b);
    }
    if(this->sockfd != -1) {
        aeDeleteFileEvent(upstream->gate->loop, this->sockfd, AE_READABLE | AE_WRITABLE);
        close(this->sockfd);
        this->sockfd = -1;
    }

    upstream->LogDebug("[tcp_upstream] TryReconnect");
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        upstream->LogError("[tcp_upstream] TryReconnect failed, error='%s'", strerror(errno));
        return;
    }
    // non block
    int err = fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK);
    if (err < 0) {
        upstream->LogError("[tcp_upstream] TryReconnect failed, error='%s'", strerror(errno));
        close(sockfd);
        return;
    }
    // create event
    aeCreateFileEvent(upstream->gate->loop, sockfd, AE_READABLE, on_connect_err, (void*)this);
    aeCreateFileEvent(upstream->gate->loop, sockfd, AE_WRITABLE, on_connect_succ, (void*)this);
    err = connect(sockfd, (struct sockaddr *)&this->addr, sizeof(this->addr));
    if(err < 0 && errno != EINPROGRESS) {
        upstream->LogError("[tcp_upstream] TryReconnect fail, error='%s'", strerror(errno));
        close(sockfd);
        return;
    }
    upstream->status = upstream_status_connecting;
    this->sockfd = sockfd;
}

void TcpUpstream::RecvClientData(const char* header, size_t headerLen, const char* payload, size_t len){ 
    Upstream* upstream = this->upstream;
    size_t packetLen = headerLen + len;
    byte_array* frame = this->WillSend(packetLen);
    if (nullptr == frame) {
        upstream->LogError("[tcp_upstream] RecvClientData failed, error='WillSend'");
        return;
    }
    memcpy(frame->back(), header, headerLen);
    if (payload != nullptr) {
        memcpy(frame->back() + headerLen, payload, len);
    }
    upstream->LogDebug("[tcp_upstream] RecvClientData packetLen:%ld", packetLen);
    frame->write(packetLen);
    return;
}

byte_array* TcpUpstream::WillSend(size_t len) {
    Upstream* upstream = this->upstream;
    if(this->sendDeque.size() <= 0 || this->sendDeque.front()->capacity() < len) {
        if (len > upstream->group->config.sendBufferSize) {
            upstream->LogError("[tcp_upstream] send buffer overflow, config=%ld, expect=%ld", upstream->group->config.sendBufferSize, len);
            return nullptr;
        } 
        byte_array* b = upstream->group->AllocBuffer(upstream->group->config.sendBufferSize);
        this->sendDeque.push_front(b);
        aeCreateFileEvent(upstream->gate->loop, sockfd, AE_WRITABLE, on_write, (void*)this);
        return b;
    } else {
        aeCreateFileEvent(upstream->gate->loop, sockfd, AE_WRITABLE, on_write, (void*)this);
        return this->sendDeque.front();
    }
}



