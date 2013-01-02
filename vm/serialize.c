// TODO do something about classes and circles, mutable maps, mutable lists etc ...
// TODO maybe allow hotel code to also serialize? requires giving up stack ...

#include "trace-off.h"

static tlHandle parsesym(tlBuffer* buf) {
    char c;
    int i;
    for (i = 0;; i++) {
        c = tlBufferReadByte(buf);
        if (c == ':' || c == ',' || c == ']' || c == '}' || c == 0) break;
    }
    tlBufferRewind(buf, i + 1); // we read beyond
    char* s = malloc(i + 1);
    tlBufferRead(buf, s, i); s[i] = 0;
    trace("sym: %s", s);
    return tlSymFromTake(s, i);
}
static tlHandle parsenum(tlBuffer* buf) {
    int i;
    bool f = false;
    for (i = 0;; i++) {
        char c = tlBufferReadByte(buf);
        if (c >= '0' && c <= '9') continue;
        if (c == '-' || c == '+' || c == 'e' || c == 'E') continue;
        if (c == '.' && !f) { f = true; continue; }
        break;
    }
    if (i == 0 || i > 16) return null;
    char b[20];
    tlBufferRewind(buf, i + 1);
    tlBufferRead(buf, b, i); b[i] = 0;
    trace("number: %s (len: %d)", b, i);
    if (f) {
        return tlFLOAT(strtod(b, 0));
    } else {
        return tlINT((int)strtol(b, 0, 10));
    }
}
static tlHandle parse(tlBuffer* buf) {
    char c = tlBufferPeekByte(buf);
    trace("probing: %c", c);
    switch (c) {
        case '{': {
            tlBufferSkipByte(buf);
            trace("{");
            tlHashMap* map = tlHashMapNew();
            while (true) {
                tlHandle k = parsesym(buf);
                if (!k) break;
                c = tlBufferReadByte(buf);
                if (c != ':') { warning("no ':' %c", c); return null; }
                tlHandle v = parse(buf);
                if (!v) { warning("no value"); return null; }
                tlHashMapSet(map, k, v);
                c = tlBufferPeekByte(buf);
                if (c != ',') break;
                tlBufferSkipByte(buf);
            }
            c = tlBufferReadByte(buf);
            if (c != '}') { warning("no '}' %c", c); return null; }
            trace("}");
            tlMap* res = tlHashMapToObject(map);
            trace("map: %d", tlMapSize(res));
            return res;
        }
        case '[': {
            tlBufferSkipByte(buf);
            trace("[");
            tlArray* ls = tlArrayNew();
            while (true) {
                tlHandle h = parse(buf);
                if (!h) break;
                tlArrayAdd(ls, h);
                c = tlBufferPeekByte(buf);
                if (c != ',') break;
                tlBufferSkipByte(buf);
            }
            c = tlBufferReadByte(buf);
            if (c != ']') { warning("no ']' %c", c); return null; }
            trace("]");
            return tlArrayToList(ls);
        }
        case '#': {
            tlBufferSkipByte(buf);
            return parsesym(buf);
        }
        case '"': {
            tlBufferSkipByte(buf);
            int i;
            int escape = 0;
            for (i = 0;; i++) {
                c = tlBufferReadByte(buf);
                if (c == 0) return null;
                if (c == '"') break;
                if (c == '\\') { escape++; i++; tlBufferSkipByte(buf); }
            }
            tlBufferRewind(buf, i + 1); // correct final '"'
            if (escape) {
                int len = i - escape;
                assert(i > 0);
                char* s = malloc(i + 1);
                for (i = 0; i < len; i++) {
                    c = tlBufferReadByte(buf);
                    if (c == '\\') {
                        c = tlBufferReadByte(buf);
                        switch (c) {
                            case '0': s[i] = '\0'; break;
                            case 't': s[i] = '\t'; break;
                            case 'n': s[i] = '\n'; break;
                            case 'r': s[i] = '\r'; break;
                            case '\\': s[i] = '\\'; break;
                            default: s[i] = c; break;
                        }
                    } else {
                        s[i] = c;
                    }
                }
                s[i] = 0;
                tlBufferSkipByte(buf);
                trace("text: %s (len: %d)", s, i);
                return tlStringFromTake(s, i);
            } else {
                char* s = malloc(i + 1);
                tlBufferRead(buf, s, i); s[i] = 0;
                tlBufferSkipByte(buf);
                trace("text: %s (len: %d)", s, i);
                return tlStringFromTake(s, i);
            }
        }
        case 'n': { tlBufferReadSkip(buf, 4); return tlNull; }
        case 'f': { tlBufferReadSkip(buf, 5); return tlFalse; }
        case 't': { tlBufferReadSkip(buf, 4); return tlTrue; }
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return parsenum(buf);
    }
    warning("unparsable: %c", c);
    return null;
}

tlHandle _from_repr(tlArgs* args) {
    tlBuffer* buf = tlBufferCast(tlArgsGet(args, 0));
    if (!buf) TL_THROW("from_repr requires a Buffer");
    tlHandle res = parse(buf);
    if (!res) TL_THROW("from_repr input malformed");
    return res;
}

static bool pprint(tlBuffer* buf, tlHandle h, bool askey) {
    tlKind* kind = tl_kind(h);

    if (kind == tlStringKind) {
        tlString* str = tlStringEscape(tlStringAs(h));
        tlBufferWrite(buf, "\"", 1);
        tlBufferWrite(buf, tlStringData(str), tlStringSize(str));
        tlBufferWrite(buf, "\"", 1);
        return true;
    }
    if (kind == tlSymKind) {
        tlSym* sym = tlSymAs(h);
        if (!askey) tlBufferWrite(buf, "#", 1);
        tlBufferWrite(buf, tlSymData(sym), tlSymSize(sym));
        return true;
    }
    if (askey) return false;

    if (kind == tlListKind) {
        tlList* list = tlListAs(h);
        tlBufferWrite(buf, "[", 1);
        for (int i = 0;; i++) {
            tlHandle v = tlListGet(list, i);
            if (!v) break;
            if (i != 0) tlBufferWrite(buf, ",", 1);
            pprint(buf, v, false);
        }
        tlBufferWrite(buf, "]", 1);
        return false;
    }

    if (tlMapOrObjectIs(h)) {
        tlMap* map = (tlMap*)h;
        tlBufferWrite(buf, "{", 1);
        bool written = false;
        for (int i = 0;; i++) {
            tlHandle v = tlMapValueIter(map, i);
            if (!v) break;
            tlHandle k = tlSetGet(tlMapKeySet(map), i);
            if (pprint(buf, k, true)) {
                tlBufferWrite(buf, ":", 1);
                pprint(buf, v, false);
                tlBufferWrite(buf, ",", 1);
                written = true;
            }
        }
        if (written) tlBufferWriteRewind(buf, 1);
        tlBufferWrite(buf, "}", 1);
        return false;
    }

    if (h == tlNull) { tlBufferWrite(buf, "null", 4); return false; }
    if (h == tlFalse) { tlBufferWrite(buf, "false", 4); return false; }
    if (h == tlTrue) { tlBufferWrite(buf, "true", 4); return false; }
    if (tlUndefinedIs(h)) { tlBufferWrite(buf, "null", 4); return false; }

    int n;
    char b[128];
    if (kind == tlIntKind) {
        n = snprintf(b, sizeof(b) - 1, "%zd", tl_int(h));
        tlBufferWrite(buf, b, n);
        return false;
    }
    if (kind == tlFloatKind) {
        n = snprintf(b, sizeof(b) - 1, "%g", tl_double(h));
        tlBufferWrite(buf, b, n);
        return false;
    }
    trace("unable to encode value: %s", tl_str(h));
    tlBufferWrite(buf, "null", 4);
    return false;
}

tlHandle _to_repr(tlArgs* args) {
    tlHandle h = tlArgsGet(args, 0);
    if (!h) h = tlNull;

    tlBuffer* buf = tlBufferNew();
    pprint(buf, h, false);
    return buf;
}

static const tlNativeCbs __serialize_natives[] = {
    { "_to_repr", _to_repr },
    { "_from_repr", _from_repr },
    { 0, 0 }
};

INTERNAL void serialize_init() {
    tl_register_natives(__serialize_natives);
}

