// author: Onne Gorter, license: MIT (see license.txt)
// simple str type, wraps a char array with size and tag

#include "trace-off.h"

tlKind* tlStringKind;
static tlString* _tl_emptyString;

// TODO short strings should be "inline" saves finalizer
struct tlString {
    tlHead head;
    bool interned; // true if this is the tlString in the interned_string hashmap
    unsigned int hash;
    unsigned int len;
    const char* data;
};

// returns length of to be returned elements, and fills in (0-based!) offset on where to start
// negative indexes means size - index
// if first < 1 it is corrected to 1
// if last > size it is corrected to size
static int sub_offset(int first, int last, int size, int* offset) {
    trace("before: %d %d (%d): %d", first, last, size, last - first + 1);
    if (first < 0) first = size + first + 1;
    if (first <= 0) first = 1;
    if (first > size) return 0;

    if (last < 0) last = size + last + 1;
    if (last < first) return 0;
    if (last >= size) last = size;

    trace("after: %d %d (%d): %d", first, last, size, last - first + 1);
    if (offset) *offset = first - 1;
    return last - first + 1;
}

// returns offset from index based number
static int at_offset_raw(tlHandle v) {
    if (tlIntIs(v)) return tl_int(v) - 1;
    if (tlFloatIs(v)) return ((int)tl_double(v)) - 1;
    return -1;
}

// returns offset from index based number or -1 if not a valid number or out of range
int at_offset(tlHandle v, int size) {
    trace("before: %s (%d)", tl_str(v), size);
    if (!v || tlNullIs(v)) return 0;

    int at;
    if (tlIntIs(v)) at = tl_int(v);
    else if (tlFloatIs(v)) at = (int)tl_double(v);
    else return -1;

    if (at < 0) at = size + at + 1;
    if (at <= 0) return -1;
    if (at > size) return -1;
    trace("after: %s (%d)", tl_str(v), size);
    return at - 1;
}

// like at_offset, but returns 0 if handle is null or tlNull or out of range
static int at_offset_min(tlHandle v, int size) {
    trace("before: %s (%d)", tl_str(v), size);
    if (!v || tlNullIs(v)) return 0;

    int at;
    if (tlIntIs(v)) at = tl_int(v);
    else if (tlFloatIs(v)) at = (int)tl_double(v);
    else return -1;

    if (at < 0) at = size + at + 1;
    if (at <= 0) return 0;
    if (at > size) return size;
    trace("after: %s (%d)", tl_str(v), size);
    return at - 1;
}

// like at_offset, but returns size if handle is null or tlNull or out of range
static int at_offset_max(tlHandle v, int size) {
    trace("before: %s (%d)", tl_str(v), size);
    if (!v || tlNullIs(v)) return size;

    int at;
    if (tlIntIs(v)) at = tl_int(v);
    else if (tlFloatIs(v)) at = (int)tl_double(v);
    else return -1;

    if (at < 0) at = size + at + 1;
    if (at <= 0) return 0;
    if (at > size) return size;
    trace("after: %s (%d)", tl_str(v), size);
    return at;
}

tlString* tlStringEmpty() { return _tl_emptyString; }

tlString* tlStringFromStatic(const char* s, int len) {
    trace("%s", s);
    tlString* str = tlAlloc(tlStringKind, sizeof(tlString));
    str->data = s;
    str->len = (len > 0)?len:strlen(s);
    return str;
}

tlString* tlStringFromTake(char* s, int len) {
    trace("%s", s);
    tlString* str = tlAlloc(tlStringKind, sizeof(tlString));
    str->data = s;
    str->len = (len > 0)?len:strlen(s);
    return str;
}

tlString* tlStringFromCopy(const char* s, int len) {
    trace("%s", s);
    if (len <= 0) len = strlen(s);
    //assert(len < TL_MAX_INT);
    char *data = malloc_atomic(len + 1);
    if (!data) return null;
    memcpy(data, s, len);
    data[len] = 0;
    return tlStringFromTake(data, len);
}

const char* tlStringData(tlString *str) {
    assert(tlStringIs(str));
    return str->data;
}

// TODO this returns the byte size, not the character count! UTF8
int tlStringSize(tlString* str) {
    assert(tlStringIs(str));
    return str->len;
}

bool tlStringIsInterned(tlString* str) {
    assert(tlStringIs(str));
    return str->interned;
}

#define mmix(h,k) { k *= m; k ^= k >> r; k *= m; h *= m; h ^= k; }
static unsigned int murmurhash2a(const void * key, int len) {
    const unsigned int seed = 33;
    const unsigned int m = 0x5bd1e995;
    const int r = 24;
    unsigned int l = len;
    const unsigned char * data = (const unsigned char *)key;
    unsigned int h = seed;
    while(len >= 4) {
        unsigned int k = *(unsigned int*)data;
        mmix(h,k);
        data += 4;
        len -= 4;
    }
    unsigned int t = 0;
    switch(len) {
        case 3: t ^= data[2] << 16;
        case 2: t ^= data[1] << 8;
        case 1: t ^= data[0];
    }
    mmix(h,t);
    mmix(h,l);
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
}

#define CONT(b) ((b & 0xC0) == 0x80)
#define SKIP_OR_THROW() if (!skip) { return -i; }
static int process_utf8(const char* from, int len, char** into, int* intolen, int* intochars) {
    assert(into);
    bool skip = true;
    trace("%d", len);

    char* data = *into;
    if (!data) *into = data = malloc_atomic(len + 1);

    int j = 0;
    int i = 0;
    int chars = 0;
    for (; i < len;) {
        char c1 = from[i++];
        if ((c1 & 0x80) == 0) { // 0xxx_xxxx
            chars++;
            data[j++] = c1;
        } else if ((c1 & 0xE0) == 0xC0) { // 110x_xxxx
            if (i + 1 > len) { i--; break; }
            char c2 = from[i++];
            if ((c2 & 0xC0) == 0x80) {
                chars++;
                data[j++] = c1; data[j++] = c2;
            } else {
                SKIP_OR_THROW()
            }
        } else if ((c1 & 0xF0) == 0xE0) { // 1110_xxxx
            if (i + 2 > len) { i--; break; }
            char c2 = from[i++]; char c3 = from[i++];
            if (CONT(c2) && CONT(c3)) {
                chars++;
                data[j++] = c1; data[j++] = c2; data[j++] = c3;
            } else {
                SKIP_OR_THROW()
            }
        } else if ((c1 & 0xF8) == 0xF0) { // 1111_0xxx
            if (i + 3 > len) { i--; break; }
            char c2 = from[i++]; char c3 = from[i++]; char c4 = from[i++];
            if (CONT(c2) && CONT(c3) && CONT(c4)) {
                chars++;
                data[j++] = c1; data[j++] = c2; data[j++] = c3; data[j++] = c4;
            } else {
                SKIP_OR_THROW()
            }
        } else if ((c1 & 0xFC) == 0xF8) { // 1111_10xx
            if (i + 4 > len) { i--; break; }
            char c2 = from[i++]; char c3 = from[i++]; char c4 = from[i++]; char c5 = from[i++];
            if (CONT(c2) && CONT(c3) && CONT(c4) && CONT(c5)) {
                chars++;
                data[j++] = c1; data[j++] = c2; data[j++] = c3; data[j++] = c4; data[j++] = c5;
            } else {
                SKIP_OR_THROW()
            }
        } else if ((c1 & 0xFE) == 0xFC) { // 1111_110x
            if (i + 5 > len) { i--; break; }
            char c2 = from[i++]; char c3 = from[i++]; char c4 = from[i++]; char c5 = from[i++]; char c6 = from[i++];
            if (CONT(c2) && CONT(c3) && CONT(c4) && CONT(c5) && CONT(c6)) {
                chars++;
                data[j++] = c1; data[j++] = c2; data[j++] = c3; data[j++] = c4; data[j++] = c5; data[j++] = c6;
            } else {
                SKIP_OR_THROW()
            }
        } else {
            SKIP_OR_THROW()
        }
    }
    trace("i: %d, j: %d, len: %d, chars: %d", i, j, len, chars);
    assert(i <= len);
    data[j] = 0;

    if (into) *into = data;
    if (intolen) *intolen = j;
    if (intochars) *intochars = chars;
    return i;
}
unsigned int tlStringHash(tlString* str) {
    assert(tlStringIs(str) || tlBinIs(str));
    if (str->hash) return str->hash;
    str->hash = murmurhash2a(str->data, str->len);
    if (!str->hash) str->hash = 1;
    return str->hash;
}
bool tlStringEquals(tlString* left, tlString* right) {
    if (left->len != right->len) return 0;
    if (left->hash && right->hash && left->hash != right->hash) return 0;
    return strcmp(left->data, right->data) == 0;
}
int tlStringCmp(tlString* left, tlString* right) {
    return strcmp(left->data, right->data);
}

tlString* tlStringIntern(tlString* str) {
    if (str->interned) return str;
    //assert(str->len < 512);
    return tlStringFromSym(tlSymFromString(str));
}

tlString* tlStringSub(tlString* from, int offset, int len) {
    if (tlStringSize(from) == len) return from;
    if (len == 0) return tlStringEmpty();

    assert(len > 0);
    assert(offset >= 0);
    assert(offset + len <= tlStringSize(from));

    char* data = malloc_atomic(len + 1);
    memcpy(data, from->data + offset, len);
    data[len] = 0;

    return tlStringFromTake(data, len);
}

tlString* tlStringCat(tlString* left, tlString* right) {
    int size = tlStringSize(left) + tlStringSize(right);
    char* data = malloc_atomic(size + 1);
    if (!data) return null;
    memcpy(data, left->data, tlStringSize(left));
    memcpy(data + tlStringSize(left), right->data, tlStringSize(right));
    data[size] = 0;
    return tlStringFromTake(data, size);
}

// generated by parser for "foo: $foo" etc...
// TODO args.map(_.toString) do it more correctly invoking toString methods
INTERNAL tlHandle _String_cat(tlTask* task, tlArgs* args) {
    int argc = tlArgsSize(args);
    trace("%d", argc);
    tlList* list = null;
    int size = 0;
    for (int i = 0; i < argc; i++) {
        tlHandle v = tlArgsGet(args, i);
        trace("%d: %s", i, tl_str(v));
        if (tlStringIs(v)) {
            size += tlStringAs(v)->len;
        } else {
            if (!list) list = tlListNew(argc);
            tlString* str = tlStringFromCopy(tl_str(tlArgsGet(args, i)), 0);
            size += str->len;
            tlListSet_(list, i, str);
        }
    }

    char* data = malloc_atomic(size + 1);
    if (!data) return null;
    size = 0;
    for (int i = 0; i < argc; i++) {
        const char* s;
        int len;
        tlHandle v = tlArgsGet(args, i);
        if (tlStringIs(v)) {
            s = tlStringAs(v)->data;
            len = tlStringAs(v)->len;
        } else {
            tlString* str = tlStringAs(tlListGet(list, i));
            s = str->data;
            len = str->len;
        }
        memcpy(data + size, s, len);
        size += len;
    }
    data[size] = 0;
    return tlStringFromTake(data, size);
}

/// object String: represents a series of characters
/// toString: returns itself
INTERNAL tlHandle _string_toString(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    return str;
}
INTERNAL tlHandle _string_toSym(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    return tlSymFromString(str);
}
INTERNAL tlHandle _string_isInterned(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    return tlBOOL(str->interned);
}
INTERNAL tlHandle _string_intern(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    if (str->len > 512) TL_THROW("can only intern strings < 512 bytes");
    return tlStringIntern(str);
}
/// size: return the amount of characters in the #String
INTERNAL tlHandle _string_size(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    return tlINT(tlStringSize(str));
}
/// bytes: return the amount of bytes in the #String, bytes >= size due to multibyte characters
INTERNAL tlHandle _string_bytes(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    return tlINT(tlStringSize(str));
}
/// hash: return the hashcode for this #String
INTERNAL tlHandle _string_hash(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    // TODO this can overflow/underflow ... sometimes, need to fix
    return tlINT(tlStringHash(str));
}

INTERNAL tlHandle _string_find_backward(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));

    int at = 1;
    tlHandle afrom = tlArgsMapGet(args, tlSYM("from"));
    if (!afrom) afrom = tlArgsGet(args, at++);
    int from = at_offset_max(afrom, tlStringSize(str));
    if (from < 0) TL_THROW("from must be Number, not: %s", tl_str(afrom));

    tlHandle aupto = tlArgsMapGet(args, tlSYM("upto"));
    if (!aupto) aupto = tlArgsGet(args, at++);
    int upto = at_offset_min(aupto, tlStringSize(str));
    if (upto < 0) TL_THROW("upto must be Number, not: %s", tl_str(aupto));

    if (tlNumberIs(tlArgsGet(args, 0)) || tlCharIs(tlArgsGet(args, 0))) {
        uint8_t b = (int)tl_double(tlArgsGet(args, 0));
        const char* data = tlStringData(str);
        for (int at = from - 1; at >= upto; at--) {
            if (data[at] == b) return tlINT(at + 1);
        }
        return tlNull;
    }

    tlString* find = tlStringCast(tlArgsGet(args, 0));
    if (!find) TL_THROW("expected a String, Char or Number");

    const char* data = tlStringData(str);
    const char* needle = tlStringData(find);
    int len = tlStringSize(find);
    for (int at = from - 1; at >= upto; at--) {
        if (!strncmp(data + at, needle, len)) return tlINT(at + 1);
    }
    return tlNull;
}

/// find(s): find #s in #String, returns the position or #null if nothing was found
/// [from] start searching from this position
/// [upto] stop searching when reaching this position
/// [backward] if backward is true, search from the end of the #String
/// when #s is a #String find will return the first occurance of that string
/// when #s is a #Number or #Char it will return the first occurance of that letter
/// > "hello world".find('o') == 5
/// > "hello world".find("o", backward=true) == 8
INTERNAL tlHandle _string_find(tlTask* task, tlArgs* args) {
    if (tl_bool(tlArgsMapGet(args, tlSYM("backward")))) return _string_find_backward(task, args);

    tlString* str = tlStringAs(tlArgsTarget(args));

    int at = 1;
    tlHandle afrom = tlArgsMapGet(args, tlSYM("from"));
    if (!afrom) afrom = tlArgsGet(args, at++);
    int from = at_offset_min(afrom, tlStringSize(str));
    if (from < 0) TL_THROW("from must be Number, not: %s", tl_str(afrom));

    tlHandle aupto = tlArgsMapGet(args, tlSYM("upto"));
    if (!aupto) aupto = tlArgsGet(args, at++);
    int upto = at_offset_max(aupto, tlStringSize(str));
    if (upto < 0) TL_THROW("upto must be Number, not: %s", tl_str(aupto));

    if (tlNumberIs(tlArgsGet(args, 0)) || tlCharIs(tlArgsGet(args, 0))) {
        uint8_t b = (int)tl_double(tlArgsGet(args, 0));
        const char* data = tlStringData(str);
        for (int at = from; at <= upto; at++) {
            if (data[at] == b) return tlINT(at + 1);
        }
        return tlNull;
    }

    tlString* find = tlStringCast(tlArgsGet(args, 0));
    if (!find) TL_THROW("expected a String, Char or Number");

    const char* p = strnstr(tlStringData(str) + from, tlStringData(find), upto - from);
    if (!p) return tlNull;
    return tlINT(1 + p - tlStringData(str));
}

/// cat: concatenate multiple #"String"s together
INTERNAL tlHandle _string_cat(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    tlString* add = tlStringCast(tlArgsGet(args, 0));
    if (!add) TL_THROW("arg must be a String");

    return tlStringCat(str, add);
}

/// get: return character at args[1] characters are just #"Int"s
INTERNAL tlHandle _string_get(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));

    int at = at_offset(tlArgsGet(args, 0), tlStringSize(str));
    if (at < 0) return tlUndef();
    return tlCHAR(str->data[at]);
}

/// lower: return a new #String with only lower case characters, only works properly for ascii str
INTERNAL tlHandle _string_lower(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    int size = tlStringSize(str);

    tlString* res = tlStringFromCopy(tlStringData(str), size);
    char* buf = (char*)res->data;
    for (int i = 0; i < size; i++) {
        buf[i] = tolower(buf[i]);
    }
    return res;
}

/// upper: return a new #String with all upper case characters, only works properly for ascii str
INTERNAL tlHandle _string_upper(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    int size = tlStringSize(str);

    tlString* res = tlStringFromCopy(tlStringData(str), size);
    char* buf = (char*)res->data;
    for (int i = 0; i < size; i++) {
        buf[i] = toupper(buf[i]);
    }
    return res;
}

/// slice: return a subsection of the #String withing bounds of args[1], args[2]
INTERNAL tlHandle _string_slice(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    int first = tl_int_or(tlArgsGet(args, 0), 1);
    int last = tl_int_or(tlArgsGet(args, 1), -1);

    int offset;
    int len = sub_offset(first, last, tlStringSize(str), &offset);
    return tlStringSub(str, offset, len);
}

static inline char escape(char c, bool cstr) {
    switch (c) {
        case '\0': return '0';
        case '\t': return 't';
        case '\n': return 'n';
        case '\r': return 'r';
        case '"': return '"';
        case '\\': return '\\';
        case '$': if (cstr) return 0; else return '$';
        default:
            //assert(c >= 32);
            return 0;
    }
}

tlString* tlStringEscape(tlString* str, bool cstr) {
    const char* data = tlStringData(str);
    int size = tlStringSize(str);
    int slashes = 0;
    for (int i = 0; i < size; i++) if (escape(data[i], cstr)) slashes++;
    if (slashes == 0) return str;

    char* ndata = malloc(size + slashes + 1);
    int j = 0;
    for (int i = 0; i < size; i++, j++) {
        char c;
        if ((c = escape(data[i], cstr))) {
            ndata[j++] = '\\';
            ndata[j] = c;
        } else {
            ndata[j] = data[i];
        }
    }
    assert(size + slashes == j);
    return tlStringFromTake(ndata, j);
}

/// escape: return a #String that has all special characters escaped by a '\'
INTERNAL tlHandle _string_escape(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    return tlStringEscape(str, tl_bool(tlArgsGet(args, 0)));
}

static bool containsChar(tlString* chars, int c) {
    for (int i = 0; i < chars->len; i++) {
        if (chars->data[i] == c) return true;
    }
    return false;
}
/// trim([chars]): return a #String that has all leading and trailing whitespace removed
/// when given optional #chars remove those characters instead
INTERNAL tlHandle _string_trim(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    tlString* chars = tlStringCast(tlArgsGet(args, 0));

    if (tlStringSize(str) == 0) return str;
    const char* data = tlStringData(str);
    int len = tlStringSize(str);
    int left = 0;
    int right = len - 1;

    if (chars) {
        while (left <= right && containsChar(chars, data[left])) left++;
        while (right > left && containsChar(chars, data[right])) right--;
    } else {
        while (left <= right && data[left] <= 32) left++;
        while (right > left && data[right] <= 32) right--;
    }
    right++; // always a zero byte

    trace("%d %d", left, right);
    if (left == len) return _tl_emptyString;
    if (left == 0 && right == len) return str;

    int size = right - left;
    assert(size > 0 && size < tlStringSize(str));
    char* ndata = malloc(size + 1);
    memcpy(ndata, data + left, size);
    ndata[size] = 0;
    return tlStringFromTake(ndata, size);
}

INTERNAL tlHandle _string_reverse(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    int size = tlStringSize(str);
    if (size <= 1) return str;

    char* data = malloc_atomic(size + 1);
    const char* from = tlStringData(str);
    for (int i = 0; i < size; i++) {
        data[i] = from[size - i - 1];
    }
    data[size] = 0;
    return tlStringFromTake(data, size);
}

static int intmin(int left, int right) { return (left<right)?left:right; }

/// startsWith: return true when this #String begins with the #String in args[1]
INTERNAL tlHandle _string_startsWith(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    tlString* start = tlStringCast(tlArgsGet(args, 0));
    if (!start) TL_THROW("arg must be a String");

    int from = at_offset((tlArgsGet(args, 1)), tlStringSize(str));
    if (from == -1) return tlFalse;

    int strsize = tlStringSize(str);
    int startsize = tlStringSize(start);
    if (strsize + from < startsize) return tlFalse;

    int r = strncmp(tlStringData(str) + from, tlStringData(start), startsize);
    return tlBOOL(r == 0);
}

/// endsWith: return true when this #String ends with the #String in args[1]
INTERNAL tlHandle _string_endsWith(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    tlString* start = tlStringCast(tlArgsGet(args, 0));
    if (!start) TL_THROW("arg must be a String");

    int strsize = tlStringSize(str);
    int startsize = tlStringSize(start);
    if (strsize < startsize) return tlFalse;

    int r = strncmp(tlStringData(str) + strsize - startsize, tlStringData(start), startsize);
    return tlBOOL(r == 0);
}

INTERNAL const char* stringtoString(tlHandle v, char* buf, int size) {
    return tlStringData(tlStringAs(v));
}
INTERNAL unsigned int stringHash(tlHandle v) {
    return tlStringHash(tlStringAs(v));
}
INTERNAL int stringEquals(tlHandle left, tlHandle right) {
    return tlStringEquals(tlStringAs(left), tlStringAs(right));
}
INTERNAL tlHandle stringCmp(tlHandle left, tlHandle right) {
    return tlCOMPARE(tlStringCmp(tlStringAs(left), tlStringAs(right)));
}
static tlKind _tlStringKind = {
    .name = "String",
    .toString = stringtoString,
    .hash = stringHash,
    .equals = stringEquals,
    .cmp = stringCmp,
};

static void string_init() {
    _tl_emptyString = tlSTR("");
    tlStringKind->klass = tlClassObjectFrom(
        "class", null,
        "toString", _string_toString,
        "toSym", _string_toSym,
        "intern", _string_intern,
        "isInterned", _string_isInterned,
        "size", _string_size,
        "bytes", _string_bytes,
        "hash", _string_hash,
        "startsWith", _string_startsWith,
        "endsWith", _string_endsWith,
        "find", _string_find,
        "get", _string_get,
        "lower", _string_lower,
        "upper", _string_upper,
        "slice", _string_slice,
        "cat", _string_cat,
        "escape", _string_escape,
        "trim", _string_trim,
        "reverse", _string_reverse,
        "times", null,
        "each", null,
        "map", null,
        "random", null,
        "split", null,
        "replace", null,
        "eval", null,
        null
    );
    tlObject* constructor = tlClassObjectFrom(
        "class", null,
        "call", null,
        "cat", _String_cat,
        "_methods", null,
        null
    );
    tlObjectSet_(constructor, s_class, tlObjectFrom("name", tlSYM("String"), null));
    tlObjectSet_(tlStringKind->klass, s_class, constructor);
    tlObjectSet_(constructor, s__methods, tlStringKind->klass);
    tl_register_global("String", constructor);
}

