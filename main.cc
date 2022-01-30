#include "gate.h"
#include "config.h"
#include "errors.h"
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <signal.h>
#include <getopt.h>

static int actionConfigTest(Config* config) {
    fprintf(stderr, "ok\n");
    return 0;
}

static int actionServerStop(Config* config) {
    std::ifstream pidfile(config->pid);
    if (!pidfile) {
        fprintf(stderr, "Server not running\n");
        return 1;
    }
    int pid = 0;
    pidfile >> pid;
    int err = kill(pid, SIGUSR1);
    if (err) {
        fprintf(stderr, "%s %d\n", strerror(errno), pid);
        return err;
    } else {
        fprintf(stderr, "%d killed\n", pid);
    }
    return 0;
}

static int actionServerReload(Config* config) {
    std::ifstream pidfile(config->pid);
    if (!pidfile) {
        fprintf(stderr, "No Such File %s\n", config->pid.c_str());
        return 1;
    }
    int pid = 0;
    pidfile >> pid;
    int err = kill(pid, SIGUSR2);
    if (err) {
        fprintf(stderr, "%s %d\n", strerror(errno), pid);
        return err;
    }
    return 0;
}

static int actionHelp() {
    printf(R"(longg version: 
Usage: longg

Options:
  -?,-h         : this help
)");
    return 0;
}

static const char* shortopts = "c:s:htd";
static const struct option longopts[] = {
     {"help", no_argument, NULL, 'h'},
     {"conf", required_argument, NULL, 'c'},
     {"signal", required_argument, NULL, 's'},
     {"test", no_argument, NULL, 't'},
     {"daemon", no_argument, NULL, 'd'},
     {NULL, 0, NULL, 0}
};

int main(int argc, char** argv) {
    int c;
	int idx;
	while ((c =getopt_long(argc, argv, shortopts, longopts, &idx)) != -1){
        switch(c) {
            case 'h':{
                arguments.help = true;
            }break;
            case 'c': {
                arguments.configureFilePath = optarg;
            }break;
            case 's': {
                arguments.signal = optarg;
            }break;
            case 't': { 
                arguments.testConfigureFile = true;
            }break;
            case 'd': { 
                arguments.daemon = true;
            }break;
        }
    }
    if (arguments.configureFilePath.size() <= 0) {
        return actionHelp();
    }
    try {
        if (arguments.help) {
            return actionHelp();
        } else if(arguments.signal == "stop") {
            Config* config = NewConfig();
            int err = config->Parse(arguments.configureFilePath.c_str());
            if (err) {
                return err;
            }
            return actionServerStop(config);
        } else if(arguments.signal == "reload") {
            Config* config = NewConfig();
            int err = config->Parse(arguments.configureFilePath.c_str());
            if (err) {
                return err;
            }
            return actionServerReload(config);
        } else if(arguments.testConfigureFile) {
            Config* config = NewConfig();
            int err = config->Parse(arguments.configureFilePath.c_str());
            if (err) {
                return err;
            }
            return actionConfigTest(config);
        } else {
            gate = NewGate();
            if (nullptr == gate) {
                return e_out_of_menory;
            }
            int err = gate->Main();
            return err;
        }
    }catch(std::exception& e) {
            fprintf(stderr, "%s\n", e.what());
            return 1;
     }catch(...) {
            printf("aaaa\n");
            return 1;

    }
}
