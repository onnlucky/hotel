// starting point of hotel

#include "tl.h"
#include "debug.h"

tlHandle readref(tlBuffer* buf, tlList* data, int* val) {
    if (tlBufferSize(buf) < 1) fatal("buffer too small");

    uint8_t b1 = tlBufferReadByte(buf);
    if ((b1 & 0x80) == 0x80) {
        int lit = b1 & 0x3F;
        switch (lit) {
            case 0: return tlNull;
            case 1: return tlFalse;
            case 2: return tlTrue;
            case 3: return tlStringEmpty();
            case 4: return tlListEmpty();
            case 5: return tlMapEmpty();
            default: return tlINT(lit - 7);
        }
    }

    int at;
    if ((b1 & 0x80) == 0x00) {
        at = b1 & 0x7F;
    } else if ((b1 & 0xC0) == 0xC0) {
        assert("fucked");
        if (tlBufferSize(buf) < 1) fatal("buffer too small");
        uint8_t b2 = tlBufferReadByte(buf);
        at = (b1 & 0x1F) << 8 | b2;
    } else if ((b1 & 0xE0) == 0xE0) {
        assert("fucked");
        if (tlBufferSize(buf) < 2) fatal("buffer too small");
        uint8_t b2 = tlBufferReadByte(buf);
        uint8_t b3 = tlBufferReadByte(buf);
        at = (b1 & 0x0F) << 16 | b2 << 8 | b3;
    } else {
        fatal("reference too big");
        return null;
    }

    if (val) *val = at;
    if (data) return tlListGet(data, at);
    return null;
}

int readsizebytes(tlBuffer* buf, int bytes) {
    bytes += 1;
    if (tlBufferSize(buf) < bytes) fatal("buffer too small");
    int res = 0;
    for (int i = 0; i < bytes; i++) {
        res = res << 8 | tlBufferReadByte(buf);
    }
    print("readsizebytes: %d %d", bytes, res);
    return res;
}

tlHandle readvalue(tlBuffer* buf, tlList* data) {
    if (tlBufferSize(buf) < 1) fatal("buffer too small");
    uint8_t b1 = tlBufferReadByte(buf);

    if ((b1 & 0x80) == 0x80) { // short string
        int size = b1 & 0x7F;
        print("short string: %d", size);
        if (tlBufferSize(buf) < size) fatal("buffer too small");
        char *data = malloc_atomic(size + 1);
        tlBufferRead(buf, data, size);
        data[size] = 0;
        return tlStringFromTake(data, size);
    }
    if ((b1 & 0xE0) == 0x40) { // short list
        int size = b1 & 0x1F;
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
    if ((b1 & 0xE0) == 0x60) { // short map
        int size = b1 & 0x1F;
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

    int sizebytes = b1 & 7;
    int size = readsizebytes(buf, sizebytes);
    switch (b1 & 0xF8) {
        case 0x00: // number
            print("number");
            break;
        case 0x07: // float
            print("number");
            break;
        case 0x10: // bignum
            print("number");
            break;
        case 0x17: // string
            print("string");
            break;
        case 0x20: // list
            print("list");
            break;
        case 0x27: // map
            print("map");
            break;
        default:
            fatal("unknown value type: %d", b1);
            break;
    }
    print("something: %d %d", sizebytes, size);
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

    deserialize(tlBufferFromFile("test.tlb"));
    return 0;

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

