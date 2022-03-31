#pragma once 

#include <cstdlib>

size_t Base64EncodeLen(size_t len);
size_t Base64DecodeLen(size_t len);

int Base64Encode(const unsigned char* src, size_t srcLen, unsigned char* dst, size_t* dstLen);
int Base64Encode(const char* src, size_t srcLen, char* dst, size_t* dstLen);

int Base64Decode(const unsigned char* src, size_t srcLen, unsigned char* dst, size_t* dstLen);

