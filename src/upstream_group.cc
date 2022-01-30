#include "upstream_group.h"
#include "upstream.h"
#include "gate.h"
#include "config.h"
#include "client.h"
#include "byte_array.h"
#include <cstdlib>

enum group_status {
    group_status_none          = 0,
    group_status_start         = 1,
    group_status_closing       = 2,
    group_status_closed        = 3,
};

static int _aeHeartbeatProc(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    UpstreamGroup* upstreamGroup = (UpstreamGroup*)clientData;
    upstreamGroup->TryHeartbeat();
    return upstreamGroup->config.heartbeat*1000;
}

static int _aeReconnectProc(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    UpstreamGroup* upstreamGroup = (UpstreamGroup*)clientData;
    upstreamGroup->TryReconnect();
    return upstreamGroup->config.reconnect*1000;
}

static void _aeEventFinalizerProc(struct aeEventLoop *eventLoop, void *clientData) {

}

UpstreamGroup* NewUpstreamGroup(Gate* gate, UpstreamGroupConfig& config) {
    UpstreamGroup* self = new UpstreamGroup(gate, config);
    return self;
}

UpstreamGroup::UpstreamGroup(Gate* gate, UpstreamGroupConfig& config) {
    this->gate = gate;
    this->config = config;
    this->status = group_status_none;
    this->reconnectTimerId = -1;
    this->heartbeatTimerId = -1;
}

UpstreamGroup::~UpstreamGroup() {
    this->gate->LogDebug("[upstream_group] ~UpstreamGroup");
    if (this->heartbeatTimerId >= 0) {
        aeDeleteTimeEvent(this->gate->loop, this->heartbeatTimerId);
        this->heartbeatTimerId = -1;
    }
    if (this->reconnectTimerId >= 0) {
        aeDeleteTimeEvent(this->gate->loop, this->reconnectTimerId);
        this->reconnectTimerId = -1;
    }
    for (auto& it : this->upstreamArr) {
        delete it;
    }
    this->upstreamArr.clear();
    this->upstreamDict.clear();
    for (auto& it : this->freeBufferArr) {
        delete it;
    }
    this->freeBufferArr.clear();
}

int UpstreamGroup::Reload(UpstreamGroupConfig& config) {
    this->config.sendBufferSize = config.sendBufferSize;
    this->config.recvBufferSize = config.recvBufferSize;
    // 重载timer
    if (this->config.heartbeat != config.heartbeat) {
        this->config.heartbeat = config.heartbeat;
        if(this->heartbeatTimerId >= 0) {
            aeDeleteTimeEvent(this->gate->loop, this->heartbeatTimerId);
            this->heartbeatTimerId = -1;
        }
    }
    if (this->config.reconnect != config.reconnect) {
        this->config.reconnect = config.reconnect;
        if (this->reconnectTimerId >= 0) {
            aeDeleteTimeEvent(this->gate->loop, this->reconnectTimerId);
            this->reconnectTimerId = -1;
        }
    }
    int err = this->initTimer();
    if (err) {
        return err;
    }

    // 新增upstream
    for (auto& it : config.upstreamDict) {
        if (this->upstreamDict.find(it.second.name) == this->upstreamDict.end()) {
            int err = this->addUpstream(it.second);
            if (err) {
                return err;
            }
        }
    }
    std::vector<Upstream*> removeUpstreamArr;
    for (auto& it : this->upstreamDict) {
        const auto& c = config.upstreamDict.find(it.second->config.name);
        if (c  == config.upstreamDict.end()) {
            removeUpstreamArr.push_back(it.second);
        } else {
            it.second->Reload(c->second);
        }
    }
    for (auto& it : removeUpstreamArr) {
        this->removeUpstream(it);
    }

    return 0;
}


int UpstreamGroup::initTimer() {
    if (this->heartbeatTimerId < 0 && this->config.heartbeat > 0) {
        this->heartbeatTimerId = aeCreateTimeEvent(this->gate->loop, this->config.heartbeat*1000, _aeHeartbeatProc, this, _aeEventFinalizerProc);
        if (this->heartbeatTimerId == AE_ERR) {
            return -1;
        }
    }
    if (this->reconnectTimerId < 0 && this->config.reconnect > 0) {
        this->reconnectTimerId = aeCreateTimeEvent(this->gate->loop, this->config.reconnect*1000, _aeReconnectProc, this, _aeEventFinalizerProc);
        if (this->reconnectTimerId == AE_ERR) {
            return -1;
        }
    }
    return 0;
}

int UpstreamGroup::removeUpstream(Upstream* upstream) {
    this->gate->LogDebug("[upstream_group] remove upstream %s", upstream->config.name.c_str());
    upstream->Shutdown();
    return 0;
}

int UpstreamGroup::addUpstream(UpstreamConfig& config) {
    this->gate->LogDebug("[upstream_group] add upstream %s", config.name.c_str());
    Upstream* upstream = NewUpstream(this->gate, this, config);
    int err = upstream->Start();
    if (err) {
        return err;
    }
    this->upstreamArr.push_back(upstream);
    this->upstreamDict[config.name] = upstream;
    return 0;
}

int UpstreamGroup::Start() {
    this->gate->LogDebug("[upstream_group] Start, %ld", this->config.upstreamDict.size());
    for (auto& it : this->config.upstreamDict) {
        int err = this->addUpstream(it.second);
        if (err) {
            return err;
        }
    }
    int err = this->initTimer();
    if (err) {
        return err;
    }
    this->status = group_status_start;
    return 0;
}

byte_array* UpstreamGroup::AllocBuffer(size_t size) {
    if(this->freeBufferArr.size() <= 0) {
        byte_array* b = new byte_array(size);
        if(b == nullptr) {
            return nullptr;
        }
        this->freeBufferArr.push_back(b);
    }
    byte_array* b = this->freeBufferArr.back();
    b->reset();
    this->freeBufferArr.pop_back();
    return b;
}

void UpstreamGroup::FreeBuffer(byte_array* b) {
    this->freeBufferArr.push_back(b);
}


void UpstreamGroup::TryReconnect() {
    for (auto upstream: this->upstreamArr) {
        upstream->TryReconnect();
    }
}

void UpstreamGroup::TryHeartbeat() {
    for (auto upstream: this->upstreamArr) {
        upstream->TryHeartbeat();
    }
}

Upstream* UpstreamGroup::SelectUpstream() {
    int totalWeight = 0;
    for (auto it : this->upstreamArr) {
        totalWeight += it->config.weight;
    }
    this->gate->LogDebug("[upstream_group] SelectUpstream, total=%d", totalWeight);
    if (totalWeight == 0) {
        return nullptr;
    }
    int guess = rand() % totalWeight;
    int weight = 0;
    size_t index = -1;
    for (auto it : this->upstreamArr) {
        index++;
        weight += it->config.weight;
        if (guess < weight) {
            break;
        }
    }
    this->gate->LogDebug("[upstream_group] SelectUpstream, total=%d, guess=%d, index=%d", totalWeight, guess, index);
    if (index < 0 || index >= this->upstreamArr.size()) {
        return nullptr;
    }
    Upstream* upstream = this->upstreamArr[index];
    return upstream;
}

void UpstreamGroup::Shutdown() {
    if(this->status != group_status_start) {
        delete this;
        return;
    }
    this->status = group_status_closing;
    for (auto upstream: this->upstreamArr) {
        upstream->groupShutdown();
    }
}

void UpstreamGroup::onUpstreamClose(Upstream* upstream) {
    this->gate->LogDebug("[upstream_group] onUpstreamClose");
    if (this->status != group_status_closing) {
        return;
    }
    this->gate->UpstreamRemove(upstream);

    this->upstreamDict.erase(upstream->config.name);
    bool found = false;
    for (auto it = this->upstreamArr.begin(); it != this->upstreamArr.end(); ++it) {
        if (*it == upstream) {
            delete *it;
            this->upstreamArr.erase(it);
            found = true;
            break;
        }
    }
    if (!found) {
        this->gate->LogError("[upstream_group] onUpstreamClose, error='upstream not found'");
    }
   
    if (this->upstreamArr.size() <= 0) {
        delete this;
    }
    
}




