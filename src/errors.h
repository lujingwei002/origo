#pragma once

#include <string>
#include <exception>

const int e_out_of_menory   = 1;
const int e_config_syntax   = 2;
const int e_eof             = 3;
const int e_fork            = 4;
const int e_status          = 5;
const int e_listen          = 6;
const int e_bind            = 7;
const int e_socket          = 8;
const int e_ae              = 9;
const int e_ae_createtimer  = 10;
const int e_gate_status     = 11;
const int e_open_pid_file   = 12;
const int e_lock_pid_file   = 13;
const int e_server_status   = 14;
const int e_upstream_group_status   = 15;
const int e_unknown_client  = 16;
const int e_invalid_args    = 17;
const int e_command_not_found = 18;
const int e_gate_reload     = 19;
const int e_upstream_invalid= 20;



struct out_of_memory : std::exception {
      const char* what() const noexcept {return "out of memory!";}
};

class exception: public std::exception {
public:
    exception(std::string message) {
        this->message = message; 
    }
    const char* what() const noexcept {return this->message.c_str();}
private:
    std::string message;
};
