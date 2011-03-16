// ** simple text type, wraps a char array with size and tag **

// TODO does use byte length, not utf8 length

#include "trace-off.h"

static tText* t_empty_text;
struct tText {
    tHead head;
    intptr_t chars;
    intptr_t hash;
    intptr_t size;
    const char* bytes;
};

tText* ttext_new_global(const char* str) {
    int size = strlen(str);
    if (size == 0) return t_empty_text;

    tText* text = global_alloc(TText, 0);
    text->bytes = str;
    text->size = size;
    return text;
}

static tText* tTEXT(const char *s) { return ttext_new_global(s); }
static tSym tSYM(const char *s) { return (tSym)s[0]; }

static void ttext_free(tText *text) {
    assert(ttext_is(text));
    free(text);
}

int ttext_size(tText* text) {
    return text->size;
}

static const char * ttext_bytes(tText *text) {
    assert(ttext_is(text));
    return text->bytes;
}

struct tMem {
    tHead head;
    intptr_t size;
    const char* bytes;
};

tMem* tmem_new(tTask* task, const char* bytes, size_t size) {
    assert(bytes);
    tMem* mem = task_alloc(task, TMem, 0);
    mem->size = size;
    mem->bytes = bytes;
    return mem;
}
tMem* tmem_new_size(tTask* task, size_t size) {
    return tmem_new(task, malloc(size), size);
}

int tmem_size(tMem* mem) {
    return mem->size;
}

const char* tmem_bytes(tMem* mem) {
    return mem->bytes;
}

char* tmem_bytes_(tMem* mem) {
    return (char*)mem->bytes;
}
