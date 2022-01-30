#include "byte_array.h"
#include <memory>
#include <cassert>

byte_array::byte_array(size_t size) {
    this->_data = (char*)malloc(size);
    assert(this->_data);
    this->_end = this->_data + size;
}

byte_array::~byte_array() {
    if(this->_data) {
        free(this->_data);
        this->_data = nullptr;
    }
}

void byte_array::reset() {
    this->_front = this->_data;
    this->_back = this->_data;
}
