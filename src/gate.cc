#include "gate.h"
#include "config.h"
#include "server.h"
#include "errors.h"
#include "upstream_group.h"
#include "upstream.h"
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <signal.h>
#include <unistd.h> // getpid
#include <cstdarg>
#include <cstdarg>
#include <fcntl.h> // open
#include<sys/file.h> // flock
#include <openssl/ssl.h>
#include <openssl/err.h>

Gate* gate = nullptr;

enum gate_status {
    gate_status_none    = 0,
    gate_status_start   = 1,
    gate_status_closing = 2,
    gate_status_closed  = 3,
};

Gate* NewGate() {
    Gate* self = new Gate();
    if (nullptr == self) {
        throw exception("out of memory");
    }
    return self;
}

static void sig_int(int b) {
    fprintf(stderr, "sig_int\n");
    if (gate == nullptr) {
        exit(0);
    }
    gate->shutdown();
}

static void sig_usr1(int b) {
    fprintf(stderr, "sig_usr1\n");
    if (gate == nullptr) {
        exit(0);
    }
    gate->shutdown();
}

static void sig_usr2(int b) {
    fprintf(stderr, "sig_usr2\n");
    if (gate == nullptr) {
        exit(0);
    }
    gate->reload();
}

Gate::Gate() {
    this->config = NewConfig();
    this->loop = aeCreateEventLoop(1024*1024);
    this->serverId = 0;
    this->accessLogger = nullptr;
    this->errorLogger = nullptr;
    this->debugLogger = nullptr;
    this->status = gate_status_none;
}

Gate::~Gate() {
    if(this->accessLogger) {
        this->accessLogger = nullptr;
    }
    if(this->errorLogger) {
        this->errorLogger = nullptr;
    }
    if(this->debugLogger) {
        this->debugLogger = nullptr;
    }
}

int Gate::shutdown() {
    if (this->status != gate_status_start){
        return e_gate_status;
    }
    this->status = gate_status_closing;
    aeStop(this->loop);
    return 0;
}

int Gate::reload(Config* config) {
    this->config->maintain = config->maintain;

    // 重载logger
    if (this->config->accessLog != config->accessLog && this->accessLogger != nullptr) {
        delete this->accessLogger; 
        this->accessLogger = nullptr;
        this->config->accessLog = config->accessLog;
    }
    if (this->config->errorLog != config->errorLog && this->errorLogger != nullptr) {
        delete this->errorLogger; 
        this->errorLogger = nullptr;
        this->config->errorLog = config->errorLog;
    }
    if (this->config->debugLog != config->debugLog && this->debugLogger != nullptr) {
        delete this->debugLogger; 
        this->debugLogger = nullptr;
        this->config->debugLog = config->debugLog;
    }
    int err = this->initLogger();
    if (err) {
        return err;
    }

    // 新增server
    for (auto& it : config->serverDict) {
        if (this->name2Server.find(it.second.name) == this->name2Server.end()) {
            int err = this->addServer(it.second);
            if (err) {
                return err;
            }
        }
    }

    // 删除server
    std::vector<Server*> removeServerArr;
    for (auto& it : this->serverDict) {
        const auto& c = config->serverDict.find(it.second->config.name);
        if (c  == config->serverDict.end()) {
            removeServerArr.push_back(it.second);
        } else {
            it.second->reload(c->second);
        }
    }
    for (auto&it : removeServerArr) {
        this->removeServer(it);
    }

     // 新增upstream
    for (auto& it : config->upstreamGroupDict) {
        if (this->upstreamGroupDict.find(it.second.name) == this->upstreamGroupDict.end()) {
            int err = this->addUpstreamGroup(it.second);
            if (err) {
                return err;
            }
        }
    }

    // 删除upstream
    std::vector<UpstreamGroup*> removeUpstreamGroupArr;
    for (auto& it : this->upstreamGroupDict) {
        const auto& c = config->upstreamGroupDict.find(it.second->config.name);
        if (c  == config->upstreamGroupDict.end()) {
            removeUpstreamGroupArr.push_back(it.second);
        } else {
            it.second->reload(c->second);
        }
    }
    for (auto&it : removeUpstreamGroupArr) {
        this->removeUpstreamGroup(it);
    }
    return 0;
}

int Gate::reload() {
    try {
        Config* config = NewConfig();
        int err = config->Parse(this->configureFilePath.c_str());
        if (err) {
            this->logError("reload failed, error=%d", err);
            return e_gate_reload;
        }
        err = this->reload(config);
        if (err) {
            this->logError("reload failed, error=%d", err);
            return e_gate_reload;
        }
        return 0;
    }catch(std::exception& e) {
        this->logError("reload failed, exception=%s", e.what());
        return e_gate_reload;
    }catch(...) {
        this->logError("reload failed");
        return e_gate_reload;
    }
}

int Gate::forver() {
    return 0;
}

int Gate::initSignal() {
    signal(SIGHUP, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    //往关闭的socket写数据
    signal(SIGPIPE, SIG_IGN);
    //google protobuf出错时候会出这个
    signal(SIGABRT, SIG_IGN);
    //atexit(_atexit);
    //ctrl-c信号
    signal(SIGINT, sig_int);
    signal(SIGUSR1, sig_usr1);
    signal(SIGUSR2, sig_usr2);
    return 0;
}

int Gate::initLogger() {
    if (this->accessLogger == nullptr && this->config->accessLog.length() > 0) {
        Logger* accessLogger = NewLogger(this->config->accessLog.c_str());
        if (nullptr == accessLogger) {
            return e_out_of_menory;
        }
        int err = accessLogger->start();
        if (err) {
            delete accessLogger;
            return err;
        }
        this->accessLogger = accessLogger;
    }
    if (this->errorLogger == nullptr && this->config->errorLog.length() > 0) {
        Logger* errorLogger = NewLogger(this->config->errorLog.c_str());
        if (nullptr == errorLogger) {
            return e_out_of_menory;
        }
        int err = errorLogger->start();
        if (err) {
            delete errorLogger;
            return err;
        }
        this->errorLogger = errorLogger;
    }
    if (this->debugLogger == nullptr && this->config->debugLog.length() > 0) {
        Logger* debugLogger = NewLogger(this->config->debugLog.c_str());
        if (nullptr == debugLogger) {
            return e_out_of_menory;
        }
        int err = debugLogger->start();
        if (err) {
            delete debugLogger;
            return err;
        }
        this->debugLogger = debugLogger;
    }
    return 0;
}

int Gate::initServer() {
    for (auto& it : this->config->serverDict) {
        int err = this->addServer(it.second);
        if (err) {
            return err;
        }
    }
    return 0;
}

int Gate::removeServer(Server* server) {
    this->logDebug("[gate] remove server %s", server->config.name.c_str());
    this->serverDict.erase(server->serverId);
    this->name2Server.erase(server->config.name);
    return server->shutdown();
}

int Gate::addServer(ServerConfig& config) {
    uint64_t serverId = ++this->serverId;
    Server* server = NewServer(this, serverId, config);
    int err = server->start();
    if (err) {
        delete server;
        return err;
    }
    this->serverDict[serverId] = server;
    this->name2Server[config.name] = server;
    return 0;
}

int Gate::initUpstream() {
    for (auto& it : this->config->upstreamGroupDict) {
        int err = this->addUpstreamGroup(it.second);
        if (err) {
            return err;
        }
    }
    return 0;
}

void Gate::onUpstreamRemove(Upstream* upstream) {
    for (auto& it : this->serverDict) {
        it.second->onUpstreamRemove(upstream);
    }
}

int Gate::removeUpstreamGroup(UpstreamGroup* group) {
    this->logDebug("[gate] remove upstream group %s", group->config.name.c_str());
    this->upstreamGroupDict.erase(group->config.name);
    return group->shutdown();
}

int Gate::addUpstreamGroup(UpstreamGroupConfig& config) {
    UpstreamGroup* group = NewUpstreamGroup(this, config);
    if (group == nullptr) {
        return e_out_of_menory;
    }
    int err = group->start();
    if (err) {
        delete group;
        return err;
    }
    this->upstreamGroupArr.push_back(group);
    this->upstreamGroupDict[group->config.name] = group;
    return 0;
}

int Gate::initDaemon() {
    bool daemon = false;
    if (arguments.daemon) daemon = true;
    if (this->config->daemon) daemon = true;
    if (daemon) {
        int pid;
        pid = fork();
        if (pid > 0) {
            exit(0);
        } else if(pid < 0){
            return e_fork;
        }
        setsid();
        pid = fork();
        if(pid > 0){
            exit(0);
        } else if(pid < 0){
            return e_fork;
        }
    }
    return 0;
}

int Gate::tryLockPid() {
    int fd = open(this->config->pid.c_str(), O_RDWR|O_CREAT, 0755);
    if (fd < 0) {
        fprintf(stderr, "open pid failed, path=%s", this->config->pid.c_str());
        return e_open_pid_file;
    }
    if (flock(fd, LOCK_EX|LOCK_NB) < 0) {
        fprintf(stderr, "lock pid failed, path=%s", this->config->pid.c_str());
        return e_lock_pid_file;
    }
    return 0;
}

int Gate::run() {
    std::ofstream pidfile(this->config->pid);
    pidfile << getpid() << std::endl;
    pidfile.close();

    this->status = gate_status_start;
    aeMain(this->loop);
    this->status = gate_status_closed;

    remove(this->config->pid.c_str());
    return 0;
}

int Gate::initSSL() {
    SSL_library_init();             /*  为SSL加载加密和哈希算法 */ 
    SSL_load_error_strings();       /*  为了更友好的报错，加载错误码的描述字符串 */ 
    ERR_load_BIO_strings();         /*  加载 BIO 抽象库的错误信息 */
    OpenSSL_add_all_algorithms();   /*  加载所有 加密 和 散列 函数 */
    return 0;
}


int Gate::main() {
    if (this->status != gate_status_none) {
        return e_gate_status;
    }
    this->configureFilePath = arguments.configureFilePath;
    int err = this->config->Parse(this->configureFilePath.c_str());
    if (err) {
        return err;
    }
    err = chdir(this->config->workDir.c_str());
    if (err) {
        return err;
    }
    err = this->tryLockPid();
    if (err) {
        return err;
    }
    err = this->initDaemon();
    if (err) {
        return err;
    }
    err = this->initSignal();
    if (err) {
        return err;
    }
    err = this->initSSL();
    if (err) {
        return err;
    }
    std::cout << this->config->DebugString() << std::endl;
    err = this->initLogger();
    if (err) {
        return err;
    }
    err = this->initServer();
    if (err) {
        return err;
    }
    err = this->initUpstream();
    if (err) {
        return err;
    }
    return this->run();
}

UpstreamGroup* Gate::selectUpstreamGroup(std::string& path) {
    auto it = this->upstreamGroupDict.find(path);
    if (it == this->upstreamGroupDict.end()) {
        return nullptr;
    }
    return it->second;
}

void Gate::recvUpstreamData(Upstream* upstream, uint64_t sessionId, const char* data, size_t len) {
    this->logDebug("[gate] recvUpstreamData");
    uint64_t serverId = sessionId >> 50;
    auto it = this->serverDict.find(serverId);
    if (it == this->serverDict.end()) {
        this->logError("[gate] %ld server not found when recv upstream data, %s %s", serverId, upstream->group->config.name.c_str(), upstream->config.name.c_str());
        return;
    }
    it->second->recvUpstreamData(upstream, sessionId, data, len); 
}

void Gate::recvUpstreamKick(Upstream* upstream, uint64_t sessionId, const char* data, size_t len) {
    this->logDebug("[gate] recvUpstreamKick");
    uint64_t serverId = sessionId >> 50;
    auto it = this->serverDict.find(serverId);
    if (it == this->serverDict.end()) {
        this->logError("[gate] %ld server not found when recv upstream kick, %s %s", serverId, upstream->group->config.name.c_str(), upstream->config.name.c_str());
        return;
    }
    it->second->recvUpstreamKick(upstream, sessionId, data, len); 
}

void Gate::logAccess(const char* fmt, ...) {
    if(nullptr == this->accessLogger) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    this->accessLogger->Log(fmt, args);
    va_end(args);
}

void Gate::logError(const char* fmt, ...) {
    if(nullptr == this->errorLogger) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    this->errorLogger->Log(fmt, args);
    va_end(args);
}

void Gate::logDebug(const char* fmt, ...) {
    if(nullptr == this->debugLogger) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    this->debugLogger->Log(fmt, args);
    va_end(args);
}

void Gate::logAccess(const char* fmt, va_list args) {
    if(nullptr == this->accessLogger) {
        return;
    }
    this->accessLogger->Log(fmt, args);
}

void Gate::logError(const char* fmt, va_list args) {
    if(nullptr == this->errorLogger) {
        return;
    }
    this->errorLogger->Log(fmt, args);
}

void Gate::logDebug(const char* fmt, va_list args) {
    if(nullptr == this->debugLogger) {
        return;
    }
    this->debugLogger->Log(fmt, args);
}



