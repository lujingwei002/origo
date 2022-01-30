#pragma once 

#include <cstdint>
#include <cstdlib>
#include <memory.h>

/* |  front back end  |*/
class byte_array {
public:
    byte_array(size_t size);
    ~byte_array();
public:
    char* front() {return _front;}
    char* back() {return _back;}
    char* end() {return _end;}
    char* data() {return _data;}
    inline int write(size_t size);
    inline int read(size_t size);
    size_t capacity() {return this->_end - this->_back;}
    size_t length() {return this->_back - this->_front;}
    inline void truncate();
public:
    void reset();
private:
    char* _data;
    char* _end;
    char* _front;
    char* _back;
};

void byte_array::truncate() {
    memmove(this->_data, this->_front, this->_back - this->_front);
    this->_back = this->_data + (this->_back - this->_front);
    this->_front = this->_data;
}

int byte_array::write(size_t size) {
    this->_back += size;
    return 0;
}

int byte_array::read(size_t size) {
    this->_front += size;
    return 0;
}



