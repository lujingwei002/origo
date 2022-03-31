#include "location.h"
#include "gate.h"
#include "server.h"
#include "errors.h"
#include "upstream_group.h"

Location* NewLocation(Gate* gate, Server* server, LocationConfig& config) {
    Location* self = new Location(gate, server, config);
    return self;
}

Location::Location(Gate* gate, Server* server, LocationConfig& config) {
    this->gate = gate;
    this->server = server;
    this->config = config;
    this->accessLogger = nullptr;
    this->errorLogger = nullptr;
}

Location::~Location() {
    if(this->accessLogger) {
        delete this->accessLogger;
        this->accessLogger = nullptr;
    }
    if(this->errorLogger) {
        delete this->errorLogger;
        this->errorLogger = nullptr;
    }
}

Upstream* Location::SelectUpstream() {
    UpstreamGroup* group = this->gate->selectUpstreamGroup(this->config.proxyPass);
    if (nullptr == group) {
        return nullptr;
    }
    return group->SelectUpstream();
}

void Location::logAccess(const char* fmt, va_list args) {
    if(nullptr != this->accessLogger) {
        this->accessLogger->Log(fmt, args);
        return;
    }
    this->server->logAccess(fmt, args);
}

void Location::logError(const char* fmt, va_list args) {
    if(nullptr != this->errorLogger) {
        this->errorLogger->Log(fmt, args);
        return;
    }
    this->server->logError(fmt, args);
}

void Location::logDebug(const char* fmt, va_list args) {
    this->server->logDebug(fmt, args);
}

int Location::initLogger() {
    if (this->accessLogger == nullptr && this->config.accessLog.length() > 0) {
        Logger* accessLogger = NewLogger(this->config.accessLog.c_str());
        if (nullptr == accessLogger) {
            return 1;
        }
        int err = accessLogger->start();
        if (err) {
            delete accessLogger;
            return err;
        }
        this->accessLogger = accessLogger;
    }
    if (this->errorLogger == nullptr && this->config.errorLog.length() > 0) {
        Logger* errorLogger = NewLogger(this->config.errorLog.c_str());
        if (nullptr == errorLogger) {
            return 1;
        }
        int err = errorLogger->start();
        if (err) {
            delete errorLogger;
            return err;
        }
        this->errorLogger = errorLogger;
    }
    return 0;
}

int Location::start() {
    int err = this->initLogger();
    if (err) {
        return err;
    }
    return 0;
}

int Location::reload(LocationConfig& config) {
    // 重载logger
    if (this->config.accessLog != config.accessLog && this->accessLogger != nullptr) {
        delete this->accessLogger; 
        this->accessLogger = nullptr;
        this->config.accessLog = config.accessLog;
    }
    if (this->config.errorLog != config.errorLog && this->errorLogger != nullptr) {
        delete this->errorLogger; 
        this->errorLogger = nullptr;
        this->config.errorLog = config.errorLog;
    }
    int err = this->initLogger();
    if (err) {
        return err;
    }

    this->config = config;
    return 0;
}



