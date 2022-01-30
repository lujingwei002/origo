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
