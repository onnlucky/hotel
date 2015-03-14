#ifndef _string_h_
#define _string_h_

#include "tl.h"

// TODO short strings should be "inline" saves finalizer
// TODO optimize various aspects? ropes? size vs len?
struct tlString {
    tlHead head;
    bool interned; // true if this is the tlString in the interned_string hashmap
    unsigned int hash;
    unsigned int len; // byte size of the string
    unsigned int chars; // character size of the string
    const char* data;
};

int tlStringSize(tlString* str);
bool tlStringEquals(tlString* left, tlString* right);
uint32_t tlStringHash(tlString* str);
int tlStringCmp(tlString* left, tlString* right);

tlString* tlStringEscape(tlString* str, bool cstr);

int tlStringCharForByte(tlString* str, int byte);
int tlStringByteForChar(tlString* str, int at);

unsigned int murmurhash2a(const void * key, int len);
int process_utf8(const char* from, int len, char** into, int* intolen, int* intochars);
void write_utf8(int c, char buf[], int* len);

const char* stringtoString(tlHandle v, char* buf, int size);
uint32_t stringHash(tlHandle v);

void string_init_first();
void string_init();

#endif
