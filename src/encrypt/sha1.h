#pragma once 

#include <cstdint>

typedef struct{
    uint32_t        state[5];
    uint32_t        count[2];
    unsigned char   buffer[64];
} Sha1Context;

void Sha1Transform(uint32_t state[5], const unsigned char buffer[64]);
void Sha1Init(Sha1Context* context);
void Sha1Update(Sha1Context* context, const unsigned char* data, uint32_t len);
void Sha1Final(unsigned char digest[20], Sha1Context* context);
void Sha1Encode(char* out, const char* input, int len);

