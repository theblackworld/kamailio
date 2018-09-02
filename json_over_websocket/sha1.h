//fork from the html socked;
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#define SHA1CircularShift(bits,word) ((((word) << (bits)) & 0xFFFFFFFF) | ((word) >> (32-(bits))))

typedef struct SHA1Context{
    unsigned Message_Digest[5];
    unsigned Length_Low;
    unsigned Length_High;
    unsigned char Message_Block[64];
    int Message_Block_Index;
    int Computed;
    int Corrupted;
}SHA1Context;

void SHA1ProcessMessageBlock(SHA1Context *context);

void SHA1Reset(SHA1Context *context);

void SHA1PadMessage(SHA1Context *context);

int SHA1Result(SHA1Context *context);

void SHA1Input(SHA1Context *context,const char *message_array,unsigned length);

char * sha1_hash(const char *source);
