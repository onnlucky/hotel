// author: Onne Gorter, license: MIT (see license.txt)

// TODO do something about classes and circles, mutable maps, mutable lists etc ...
// TODO maybe allow hotel code to also serialize? requires giving up stack ...

#include "platform.h"
#include "serialize.h"

#include "value.h"
#include "sym.h"
#include "buffer.h"
#include "string.h"

static tlHandle parsestring(tlBuffer* buf, char quote) {
    tlBufferSkipByte(buf);
    char c;
    int i;
    int escape = 0;
    for (i = 0;; i++) {
        c = tlBufferReadByte(buf);
        if (c == 0) return null;
        if (c == quote) break;
        if (c == '\\') { escape++; i++; tlBufferSkipByte(buf); }
    }
    tlBufferRewind(buf, i + 1); // correct final quote
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
        trace("string: %s (len: %d)", s, i);
        return tlStringFromTake(s, i);
    } else {
        char* s = malloc(i + 1);
        tlBufferRead(buf, s, i); s[i] = 0;
        tlBufferSkipByte(buf);
        trace("string: %s (len: %d)", s, i);
        return tlStringFromTake(s, i);
    }
}
static tlHandle parsesym(tlBuffer* buf) {
    char c;
    int i;
    for (i = 0;; i++) {
        c = tlBufferReadByte(buf);
        if (c <= 32 || c == '=' || c == ',' || c == ']' || c == '}') break;
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
    // check if all the chars are valid for numbers
    for (i = 0;; i++) {
        char c = tlBufferReadByte(buf);
        if (c >= '0' && c <= '9') continue;
        if (c == '-' || c == '+' || c == 'e' || c == 'E') continue;
        if (c == '.' && !f) { f = true; continue; }
        break;
    }

    // too many or none, no number
    if (i == 0 || i > 31) { warning("no number: %d", i); return null; }

    char b[32];
    tlBufferRewind(buf, i + 1);
    tlBufferRead(buf, b, i); b[i] = 0;
    trace("number: %s (len: %d)", b, i);
    if (f) {
        return tlFLOAT(strtod(b, 0));
    } else {
        return tlINT((int)strtol(b, 0, 10));
    }
}
static tlHandle parsekey(tlBuffer* buf) {
    trace("parsekey");
    char c;
    do { c = tlBufferReadByte(buf); } while (c && c <= 32);
    trace("probing: %c", c);
    if (!c) return null;
    tlBufferRewind(buf, 1);
    if (c == '"' || c == '\'') return tlSymFromString(parsestring(buf, c));
    return parsesym(buf);
}
static tlHandle parse(tlBuffer* buf) {
    char c;
    do { c = tlBufferPeekByte(buf, 0); } while (c && c <= 32);
    trace("probing: %c", c);
    switch (c) {
        case '{': {
            tlBufferSkipByte(buf);
            trace("{");
            tlHashMap* map = tlHashMapNew();
            while (true) {
                tlHandle k = parsekey(buf);
                if (!k) break;
                do { c = tlBufferReadByte(buf); } while (c && c <= 32);
                if (c != '=') { warning("no '=' %c", c); return null; }
                tlHandle v = parse(buf);
                if (!v) { warning("no value"); return null; }
                tlHashMapSet(map, k, v);
                do { c = tlBufferReadByte(buf); } while (c && c <= 32);
                if (c != ',') { tlBufferRewind(buf, 1); break; }
            }
            c = tlBufferReadByte(buf);
            if (c != '}') { warning("no '}' %c", c); return null; }
            trace("}");
            tlObject* res = tlHashMapToObject(map);
            trace("map: %d", tlObjectSize(res));
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
                do { c = tlBufferReadByte(buf); } while (c && c <= 32);
                if (c != ',') { tlBufferRewind(buf, 1); break; }
            }
            c = tlBufferReadByte(buf);
            if (c != ']') { warning("no ']' %c", c); return null; }
            trace("]");
            return tlArrayToList(ls);
        }
        case '"': case '\'':
            return parsestring(buf, c);
        case 'n': { tlBufferReadSkip(buf, 4); return tlNull; }
        case 'f': { tlBufferReadSkip(buf, 5); return tlFalse; }
        case 't': { tlBufferReadSkip(buf, 4); return tlTrue; }
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case '-': case '+': case '.':
            return parsenum(buf);
    }
    warning("unparsable: %c", c);
    return null;
}

tlHandle _from_repr(tlTask* task, tlArgs* args) {
    tlBuffer* buf = tlBufferCast(tlArgsGet(args, 0));
    if (!buf) TL_THROW("from_repr requires a Buffer");
    tlHandle res = parse(buf);
    if (!res) TL_THROW("from_repr input malformed");
    return res;
}

static bool pprint(tlBuffer* buf, tlHandle h, bool askey, bool print, int depth) {
    if (depth > 20) return false; // TODO instead, detect cycles
    tlKind* kind = tl_kind(h);

    if (askey && kind == tlSymKind) {
        tlSym* sym = tlSymAs(h);
        tlBufferWrite(buf, tlSymData(sym), tlSymSize(sym));
        return true;
    }
    if (kind == tlStringKind || kind == tlSymKind) {
        tlString* str = tlStringEscape(tlStringAs(h), true);
        tlBufferWrite(buf, "\"", 1);
        tlBufferWrite(buf, tlStringData(str), tlStringSize(str));
        tlBufferWrite(buf, "\"", 1);
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
            pprint(buf, v, false, print, depth + 1);
        }
        tlBufferWrite(buf, "]", 1);
        return false;
    }

    if (kind == tlSetKind) {
        tlSet* set = tlSetAs(h);
        tlBufferWrite(buf, "<[", 2);
        for (int i = 0;; i++) {
            tlHandle v = tlSetGet(set, i);
            if (!v) break;
            if (i != 0) tlBufferWrite(buf, ",", 1);
            pprint(buf, v, false, print, depth + 1);
        }
        tlBufferWrite(buf, "]>", 2);
        return false;
    }

    if (tlObjectIs(h)) {
        tlObject* map = (tlObject*)h;
        tlBufferWrite(buf, "{", 1);
        bool written = false;
        for (int i = 0;; i++) {
            tlHandle v = tlObjectValueIter(map, i);
            if (!v) break;
            tlHandle k = tlSetGet(tlObjectKeys(map), i);
            if (pprint(buf, k, true, print, depth + 1)) {
                tlBufferWrite(buf, "=", 1);
                pprint(buf, v, false, print, depth + 1);
                tlBufferWrite(buf, ",", 1);
                written = true;
            }
        }
        if (written) tlBufferWriteRewind(buf, 1);
        tlBufferWrite(buf, "}", 1);
        return false;
    }


    if (h == tlNull) { tlBufferWrite(buf, "null", 4); return false; }
    if (h == tlFalse) { tlBufferWrite(buf, "false", 5); return false; }
    if (h == tlTrue) { tlBufferWrite(buf, "true", 4); return false; }
    if (tlUndefinedIs(h)) { tlBufferWrite(buf, "null", 4); return false; }

    int n;
    char b[128];
    if (kind == tlCharKind) {
        int c = tl_int(h);
        if (c < 32) {
            if (c == '\n') n = snprintf(b, sizeof(b) - 1, "'\\n'");
            else if (c == '\r') n = snprintf(b, sizeof(b) - 1, "'\\r'");
            else if (c == '\t') n = snprintf(b, sizeof(b) - 1, "'\\t'");
            else n = snprintf(b, sizeof(b) - 1, "' '");
        } else if (c == '\\') {
            n = snprintf(b, sizeof(b) - 1, "'\\\\'");
        } else {
            n = snprintf(b, sizeof(b) - 1, "'%c'", tl_int(h));
        }
        tlBufferWrite(buf, b, n);
        return false;
    }
    if (kind == tlIntKind) {
        n = snprintf(b, sizeof(b) - 1, "%lld", (long long)tlIntToInt(h));
        tlBufferWrite(buf, b, n);
        return false;
    }
    if (kind == tlFloatKind) {
        n = snprintf(b, sizeof(b) - 1, "%g", tl_double(h));
        tlBufferWrite(buf, b, n);
        return false;
    }
    if (print) {
        const char* s = tl_str(h);
        tlBufferWrite(buf, s, strlen(s));
        return false;
    }
    trace("unable to encode value: %s", tl_str(h));
    tlBufferWrite(buf, "null", 4);
    return false;
}

tlHandle _to_repr(tlTask* task, tlArgs* args) {
    tlHandle h = tlArgsGet(args, 0);
    if (!h) h = tlNull;

    tlBuffer* buf = tlBufferNew();
    pprint(buf, h, false, false, 0);
    return buf;
}

tlHandle _repr(tlTask* task, tlArgs* args) {
    tlHandle h = tlArgsGet(args, 0);
    if (!h) h = tlNull;
    tlBuffer* buf = tlBufferNew();
    pprint(buf, h, false, true, 0);
    tlBufferWrite(buf, "\0", 1);
    return tlStringFromTake(tlBufferTakeData(buf), tlBufferSize(buf) - 1);
}
tlHandle _str(tlTask* task, tlArgs* args) {
    tlHandle h = tlArgsGet(args, 0);
    if (!h) h = tlNull;
    if (tlStringIs(h)) return h;
    return tlStringFromCopy(tl_str(h), 0);
}

char* tl_repr(tlHandle h) {
    tlBuffer* buf = tlBufferNew();
    pprint(buf, h, false, true, 0);
    tlBufferWrite(buf, "\0", 1);
    return tlBufferTakeData(buf);
}

static const tlNativeCbs __serialize_natives[] = {
    { "_to_repr", _to_repr },
    { "_from_repr", _from_repr },
    { "repr", _repr },
    { "str", _str },
    { 0, 0 }
};

void serialize_init() {
    tl_register_natives(__serialize_natives);
}

