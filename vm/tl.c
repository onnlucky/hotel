#include "tl.h"
#include "debug.h"

// 11.. ....
static tlHandle decodelit(uint8_t b1) {
    int lit = b1 & 0x3F;
    switch (lit) {
        case 0: return tlNull;
        case 1: return tlFalse;
        case 2: return tlTrue;
        case 3: return tlStringEmpty();
        case 4: return tlListEmpty();
        case 5: return tlMapEmpty();
    }
    return tlINT(lit - 8);
}

// 0... ....
// 10.. .... 0... ....
static int decoderef2(tlBuffer* buf, uint8_t b1) {
    if ((b1 & 0x80) == 0x00) return b1 & 0x7F;
    int res = b1 & 0x3F;
    for (int i = 0; i < 4; i++) {
        b1 = tlBufferReadByte(buf);
        if ((b1 & 0x80) == 0x00) {
            res = (res << 7) + (b1 & 0x7F);
            break;
        }
        assert((b1 & 0x80) == 0x80);
        res = (res << 6) + (b1 & 0x3F);
    }
    return res;
}

tlHandle readref(tlBuffer* buf, tlList* data, int* val) {
    uint8_t b1 = tlBufferReadByte(buf);

    if ((b1 & 0xC0) == 0xC0) return decodelit(b1);

    int at = decoderef2(buf, b1);
    if (val) *val = at;
    if (data) return tlListGet(data, at);
    return null;
}

static int readsize(tlBuffer* buf) {
    return decoderef2(buf, tlBufferReadByte(buf));
}

tlHandle readsizedvalue(tlBuffer* buf, tlList* data, uint8_t b1) {
    int size = readsize(buf);
    assert(size > 0 && size < 1000);
    switch (b1) {
        case 0xE0: // string
            fatal("string %d", size);
            break;
        case 0xE1: // list
            fatal("list %d", size);
            break;
        case 0xE2: // set
            fatal("set %d", size);
            break;
        case 0xE3: // map
            fatal("map %d", size);
            break;
        case 0xE4: // raw
            fatal("raw %d", size);
            break;
        case 0xE5: // bytecode
            fatal("bytecode %d", size);
            break;
        case 0xE6: // bignum
            fatal("bignum %d", size);
            break;
        default:
            fatal("unknown value type: %d", b1);
            break;
    }
    fatal("not reached");
    return null;
}

tlHandle readvalue(tlBuffer* buf, tlList* data) {
    if (tlBufferSize(buf) < 1) fatal("buffer too small");
    uint8_t b1 = tlBufferReadByte(buf);
    uint8_t type = b1 & 0xE0;
    uint8_t size = b1 & 0x1F;

    switch (type) {
        case 0x00: { // short string
            print("short string: %d", size);
            if (tlBufferSize(buf) < size) fatal("buffer too small");
            char *data = malloc_atomic(size + 1);
            tlBufferRead(buf, data, size);
            data[size] = 0;
            return tlStringFromTake(data, size);
        }
        case 0x20: { // short list
            print("short list: %d", size);
            tlList* list = tlListNew(size);
            for (int i = 0; i < size; i++) {
                tlHandle v = readref(buf, data, null);
                assert(v);
                print("LIST: %d: %s", i, tl_str(v));
                tlListSet_(list, i, v);
            }
            return list;
        }
        case 0x40: // short set
            fatal("set %d", size);
        case 0x60: { // short map
            print("short map: %d (%d)", size, tlBufferSize(buf));
            tlMap* map = tlMapEmpty();
            for (int i = 0; i < size; i++) {
                tlHandle key = readref(buf, data, null);
                assert(key);
                tlHandle v = readref(buf, data, null);
                assert(v);
                print("MAP: %s: %s", tl_str(key), tl_str(v));
                //tlMapSet_(map, i, key, v);
            }
            return map;
        }
        case 0x80: // short raw
            fatal("raw %d", size);
        case 0xA0: // short bytecode
            fatal("bytecode %d", size);
        case 0xC0: { // short size number
            assert(size <= 8); // otherwise it is a float ... not implemented yet
            print("int %d", size);
            int val = 0;
            for (int i = 0; i < size; i++) {
                val = (val << 8) + tlBufferReadByte(buf);
            }
            return tlINT(val);
        }
        case 0xE0: // sized types
            return readsizedvalue(buf, data, b1);
    }
    fatal("not reached");
    return null;
}

tlHandle deserialize(tlBuffer* buf) {
    print("parsing: %d", tlBufferSize(buf));

    if (tlBufferReadByte(buf) != 't') return null;
    if (tlBufferReadByte(buf) != 'l') return null;
    if (tlBufferReadByte(buf) != '0') return null;
    if (tlBufferReadByte(buf) != '1') return null;

    int size = 0;
    tlHandle direct = readref(buf, null, &size);
    print("tl01 %d (%s)", size, tl_str(direct));
    if (direct) { assert(size == 0); return direct; }
    assert(size < 1000); // sanity check

    tlHandle v = null;
    tlList* data = tlListNew(size);
    for (int i = 0; i < size; i++) {
        v = readvalue(buf, data);
        print("%d: %s", i, tl_str(v));
        assert(v);
        tlListSet_(data, i, v);
    }
    return v;
}

int main(int argc, char** argv) {
    tl_init();

#if 1
    deserialize(tlBufferFromFile("test-input.tlb"));
    return 0;
#endif

    const char* boot = null;
    tlArgs* args = null;

    if (argc >= 2 && !strcmp(argv[1], "--noboot")) {
        if (argc < 3) fatal("no file to run");
        boot = argv[2];

        args = tlArgsNew(tlListNew(argc - 3), null);
        for (int i = 3; i < argc; i++) {
            tlArgsSet_(args, i - 3, tlSTR(argv[i]));
        }
    } else {
        args = tlArgsNew(tlListNew(argc - 1), null);
        for (int i = 1; i < argc; i++) {
            tlArgsSet_(args, i - 1, tlSTR(argv[i]));
        }
    }

    tlVm* vm = tlVmNew();
    tlVmInitDefaultEnv(vm);
    if (boot) {
        tlTask* task = tlVmEvalFile(vm, tlSTR(boot), args);
        tlHandle h = tlTaskGetValue(task);
        if (tl_bool(h)) printf("%s\n", tl_str(h));
    } else {
        tlVmEvalBoot(vm, args);
    }

    int exitcode = tlVmExitCode(vm);
    tlVmDelete(vm);
    return exitcode;
}

