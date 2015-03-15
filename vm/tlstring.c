// author: Onne Gorter, license: MIT (see license.txt)
// simple str type, wraps a char array with size and tag

#include "tlstring.h"
#include "platform.h"
#include "value.h"

#include "trace-off.h"

tlKind* tlStringKind;
static tlString* _tl_emptyString;

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

// returns the byte size, not the character count!
int tlStringSize(tlString* str) {
    assert(tlStringIs(str));
    return str->len;
}

bool tlStringIsInterned(tlString* str) {
    assert(tlStringIs(str));
    return str->interned;
}

#define mmix(h,k) { k *= m; k ^= k >> r; k *= m; h *= m; h ^= k; }
unsigned int murmurhash2a(const void * key, int len) {
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

// TODO replacement char, invalid sequences, ...
static int bytes_utf8(char c) {
    if ((c & 0x80) == 0) return 1;    // 0xxx_xxxx
    if ((c & 0xE0) == 0xC0) return 2; // 110x_xxxx
    if ((c & 0xF0) == 0xE0) return 3; // 1110_xxxx
    if ((c & 0xF8) == 0xF0) return 4; // 1111_0xxx
    if ((c & 0xFC) == 0xF8) return 5; // 1111_10xx
    return 1;
}

static int bytes_utf8_back(const char* buf) {
    int size = 0;
    while ((buf[-size] & 0xC0) == 0x80) size++;
    return size + 1;
}

static int char_utf8(const char* buf) {
    char c = buf[0];
    if ((c & 0x80) == 0) return c;
    if ((c & 0xE0) == 0xC0) return (c & 0x1F) << 6  | (buf[1] & 0x3F);
    if ((c & 0xF0) == 0xE0) return (c & 0x0F) << 12 | (buf[1] & 0x3F) <<  6 | (buf[2] & 0x3F);
    if ((c & 0xF8) == 0xF0) return (c & 0x07) << 18 | (buf[1] & 0x3F) << 12 | (buf[2] & 0x3F) <<  6 | (buf[3] & 0x3F);
    if ((c & 0xFC) == 0xF8) return (c & 0x03) << 24 | (buf[1] & 0x3F) << 18 | (buf[2] & 0x3F) << 12 | (buf[3] & 0x3F) << 6 | (buf[4] & 0x3F);
    return 0; // error
}

#define CONT(b) ((b & 0xC0) == 0x80)
#define SKIP_OR_THROW() if (!skip) { return -i; }
int process_utf8(const char* from, int len, char** into, int* intolen, int* intochars) {
    bool skip = true;
    trace("%d", len);

    char* data = null;
    if (into) {
        data = *into;
        if (!data) *into = data = malloc_atomic(len + 1);
    }

    int j = 0;
    int i = 0;
    int chars = 0;
    for (; i < len;) {
        char c1 = from[i++];
        if ((c1 & 0x80) == 0) { // 0xxx_xxxx
            chars++;
            if (data) { data[j++] = c1; }
        } else if ((c1 & 0xE0) == 0xC0) { // 110x_xxxx
            if (i + 1 > len) { i--; break; }
            char c2 = from[i++];
            if ((c2 & 0xC0) == 0x80) {
                chars++;
                if (data) { data[j++] = c1; data[j++] = c2; }
            } else {
                SKIP_OR_THROW()
            }
        } else if ((c1 & 0xF0) == 0xE0) { // 1110_xxxx
            if (i + 2 > len) { i--; break; }
            char c2 = from[i++]; char c3 = from[i++];
            if (CONT(c2) && CONT(c3)) {
                chars++;
                if (data) { data[j++] = c1; data[j++] = c2; data[j++] = c3; }
            } else {
                SKIP_OR_THROW()
            }
        } else if ((c1 & 0xF8) == 0xF0) { // 1111_0xxx
            if (i + 3 > len) { i--; break; }
            char c2 = from[i++]; char c3 = from[i++]; char c4 = from[i++];
            if (CONT(c2) && CONT(c3) && CONT(c4)) {
                chars++;
                if (data) { data[j++] = c1; data[j++] = c2; data[j++] = c3; data[j++] = c4; }
            } else {
                SKIP_OR_THROW()
            }
        } else if ((c1 & 0xFC) == 0xF8) { // 1111_10xx
            if (i + 4 > len) { i--; break; }
            char c2 = from[i++]; char c3 = from[i++]; char c4 = from[i++]; char c5 = from[i++];
            if (CONT(c2) && CONT(c3) && CONT(c4) && CONT(c5)) {
                chars++;
                if (data) { data[j++] = c1; data[j++] = c2; data[j++] = c3; data[j++] = c4; data[j++] = c5; }
            } else {
                SKIP_OR_THROW()
            }
        } else {
            SKIP_OR_THROW()
        }
    }
    trace("i: %d, j: %d, len: %d, chars: %d", i, j, len, chars);
    assert(i <= len);
    if (data) data[j] = 0;

    if (into) *into = data;
    if (intolen) *intolen = j;
    if (intochars) *intochars = chars;
    return i;
}

uint32_t tlStringHash(tlString* str) {
    assert(tlStringIs(str) || tlBinIs(str));
    if (str->hash) return str->hash;
    str->hash = murmurhash2a(str->data, str->len);
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

// works in bytes, not characters!
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

int tlStringChars(tlString* str) {
    assert(tlStringIs(str));
    if (str->chars) return str->chars;

    int chars = 0;
    int read = process_utf8(str->data, str->len, null, null, &chars);
    assert(read == str->len);
    UNUSED(read);
    str->chars = chars;
    return str->chars;
}

// calculate byte position based on char position
// is used for lengths as well, so allow one-to-far
int tlStringByteForChar(tlString* str, int at) {
    int chars = tlStringChars(str);
    if (str->chars == str->len) return at;
    assert(at >= 0 && at <= chars);
    UNUSED(chars);

    int byte = 0;
    while (at > 0) {
        if (byte >= str->len) return byte; // extra check to never go out of bounds
        byte += bytes_utf8(str->data[byte]);
        at--;
    }
    return byte;
}

// calculate char position based on byte position
int tlStringCharForByte(tlString* str, int byte) {
    int chars = tlStringChars(str);
    if (str->chars == str->len) return byte;
    UNUSED(chars);
    assert(byte >= 0 && byte <= str->len);

    int b = 0;
    int c = 0;
    while (b < byte) {
        b += bytes_utf8(str->data[b]);
        c++;
    }
    assert(c >= 0 && c <= chars);
    return c;
}

// TODO can we cache this somehow, or provide iterators
int tlStringGet(tlString* str, int at) {
    int chars = tlStringChars(str);
    assert(at >= 0 && at < chars);
    UNUSED(chars);

    int byte = 0;
    while (at > 0) {
        if (byte >= str->len) return 0; // extra check to never go out of bounds
        byte += bytes_utf8(str->data[byte]);
        at--;
    }
    if (byte + bytes_utf8(str->data[byte]) > str->len) return 0; // extra check
    return char_utf8(str->data + byte);
}

int tlStringFindChar(tlString* str, int c, int from, int upto) {
    int chars = tlStringChars(str);
    assert(upto <= chars);
    assert(from >= 0);
    UNUSED(chars);

    int byte = 0;
    int at = 0;
    while (at < from) {
        byte += bytes_utf8(str->data[byte]);
        at++;
    }
    while (at <= upto) {
        if (c == char_utf8(str->data + byte)) return at;
        byte += bytes_utf8(str->data[byte]);
        at++;
    }
    return -1;
}

int tlStringFindCharBackward(tlString* str, int c, int from, int upto) {
    int chars = tlStringChars(str);
    assert(upto >= 0);
    assert(from <= chars);
    assert(upto <= from);
    UNUSED(chars);
    //print("%X %d %d (chars: %d, size: %d)", c, from, upto, chars, str->len);

    int byte = tlStringSize(str);
    int at = chars + 1;
    while (at > from && byte >= 0) {
        //print("skip: %d %d (%X == %X)", byte, at, c, char_utf8(str->data + byte));
        byte -= bytes_utf8_back(str->data + byte - 1);
        at--;
    }
    while (at > upto && byte >= 0) {
        at--;
        //print("back: %d %d (%X == %X)", byte, at, c, char_utf8(str->data + byte));
        if (c == char_utf8(str->data + byte)) return at;
        byte -= bytes_utf8_back(str->data + byte - 1);
    }
    return -1;
}

// generated by parser for "foo: $foo" etc...
// TODO args.map(_.toString) do it more correctly invoking toString methods
tlHandle _String_cat(tlTask* task, tlArgs* args) {
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
static tlHandle _string_toString(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    return str;
}
static tlHandle _string_toSym(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    return tlSymFromString(str);
}
static tlHandle _string_isInterned(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    return tlBOOL(str->interned);
}
static tlHandle _string_intern(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    if (str->len > 512) TL_THROW("can only intern strings < 512 bytes");
    return tlStringIntern(str);
}
/// size: return the amount of characters in the #String
static tlHandle _string_size(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    return tlINT(tlStringChars(str));
}
/// bytes: return the amount of bytes in the #String, bytes >= size due to multibyte characters
static tlHandle _string_bytes(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    return tlINT(tlStringSize(str));
}
/// hash: return the hashcode for this #String
static tlHandle _string_hash(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    // TODO this can overflow/underflow ... sometimes, need to fix
    return tlINT(tlStringHash(str));
}

static tlHandle _string_find_backward(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));

    int at = 1;
    tlHandle afrom = tlArgsGetNamed(args, tlSYM("from"));
    if (!afrom) afrom = tlArgsGet(args, at++);
    int from = at_offset_max(afrom, tlStringChars(str));
    if (from < 0) TL_THROW("from must be Number, not: %s", tl_str(afrom));

    tlHandle aupto = tlArgsGetNamed(args, tlSYM("upto"));
    if (!aupto) aupto = tlArgsGet(args, at++);
    int upto = at_offset_min(aupto, tlStringChars(str));
    if (upto < 0) TL_THROW("upto must be Number, not: %s", tl_str(aupto));

    if (tlNumberIs(tlArgsGet(args, 0)) || tlCharIs(tlArgsGet(args, 0))) {
        int c = (int)tl_double(tlArgsGet(args, 0));
        int at = tlStringFindCharBackward(str, c, from, upto);
        if (at < 0) return tlNull;
        return tlINT(at + 1);
    }

    tlString* find = tlStringCast(tlArgsGet(args, 0));
    if (!find) TL_THROW("expected a String, Char or Number");

    const char* data = tlStringData(str);
    const char* needle = tlStringData(find);
    int len = tlStringSize(find);
    from = tlStringByteForChar(str, from);
    upto = tlStringByteForChar(str, upto);
    for (int at = from - 1; at >= upto; at--) {
        if (!strncmp(data + at, needle, len)) return tlINT(tlStringCharForByte(str, at) + 1);
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
static tlHandle _string_find(tlTask* task, tlArgs* args) {
    if (tl_bool(tlArgsGetNamed(args, tlSYM("backward")))) return _string_find_backward(task, args);

    tlString* str = tlStringAs(tlArgsTarget(args));

    int at = 1;
    tlHandle afrom = tlArgsGetNamed(args, tlSYM("from"));
    if (!afrom) afrom = tlArgsGet(args, at++);
    int from = at_offset_min(afrom, tlStringChars(str));
    if (from < 0) TL_THROW("from must be Number, not: %s", tl_str(afrom));

    tlHandle aupto = tlArgsGetNamed(args, tlSYM("upto"));
    if (!aupto) aupto = tlArgsGet(args, at++);
    int upto = at_offset_max(aupto, tlStringChars(str));
    if (upto < 0) TL_THROW("upto must be Number, not: %s", tl_str(aupto));

    if (tlNumberIs(tlArgsGet(args, 0)) || tlCharIs(tlArgsGet(args, 0))) {
        int c = (int)tl_double(tlArgsGet(args, 0));
        int at = tlStringFindChar(str, c, from, upto);
        if (at < 0) return tlNull;
        return tlINT(at + 1);
    }

    tlString* find = tlStringCast(tlArgsGet(args, 0));
    if (!find) TL_THROW("expected a String, Char or Number");

    from = tlStringByteForChar(str, from);
    upto = tlStringByteForChar(str, upto);
    const char* p = strnstr(tlStringData(str) + from, tlStringData(find), upto - from);
    if (!p) return tlNull;
    int byte = p - tlStringData(str);
    return tlINT(1 + tlStringCharForByte(str, byte));
}

/// cat: concatenate multiple #"String"s together
static tlHandle _string_cat(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    tlString* add = tlStringCast(tlArgsGet(args, 0));
    if (!add) TL_THROW("arg must be a String");

    return tlStringCat(str, add);
}

/// get: return character at args[1] characters are just #"Int"s
static tlHandle _string_get(tlTask* task, tlArgs* args) {
    TL_TARGET(tlString, str);
    tlHandle vat = tlArgsGet(args, 0);
    int at = at_offset(vat, tlStringChars(str));
    if (!vat || vat == tlNull) at = 0;
    if (at < 0) return tlUndef();
    return tlCHAR(tlStringGet(str, at));
}

/// lower: return a new #String with only lower case characters, only works properly for ascii str
static tlHandle _string_lower(tlTask* task, tlArgs* args) {
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
static tlHandle _string_upper(tlTask* task, tlArgs* args) {
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
static tlHandle _string_slice(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    int first = tl_int_or(tlArgsGet(args, 0), 1);
    int last = tl_int_or(tlArgsGet(args, 1), -1);

    int offset;
    int len = sub_offset(first, last, tlStringChars(str), &offset);
    if (len == 0) return tlStringEmpty();

    // work out bytes from chars
    len = tlStringByteForChar(str, len + offset);
    offset = tlStringByteForChar(str, offset);
    return tlStringSub(str, offset, len - offset);
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
static tlHandle _string_escape(tlTask* task, tlArgs* args) {
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
static tlHandle _string_trim(tlTask* task, tlArgs* args) {
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

static tlHandle _string_reverse(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    int size = tlStringSize(str);
    if (size <= 1) return str;

    char* data = malloc_atomic(size + 1);
    const char* from = tlStringData(str);
    int byte = 0;
    while (byte < size) {
        int clen = bytes_utf8(from[byte]);
        for (int i = 0; i < clen; i++) {
            data[size - byte - clen + i] = from[byte + i];
        }
        byte += clen;
    }
    data[size] = 0;
    return tlStringFromTake(data, size);
}

static int intmin(int left, int right) { return (left<right)?left:right; }

/// startsWith: return true when this #String begins with the #String in args[1]
static tlHandle _string_startsWith(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    tlString* start = tlStringCast(tlArgsGet(args, 0));
    if (!start) TL_THROW("arg must be a String");

    tlHandle vfrom = tlArgsGet(args, 1);
    int from = at_offset(vfrom, tlStringSize(str));
    if (!vfrom || vfrom == tlNull) from = 0;
    if (from == -1) return tlFalse;

    int strsize = tlStringSize(str);
    int startsize = tlStringSize(start);
    if (strsize + from < startsize) return tlFalse;

    int r = strncmp(tlStringData(str) + from, tlStringData(start), startsize);
    return tlBOOL(r == 0);
}

/// endsWith: return true when this #String ends with the #String in args[1]
static tlHandle _string_endsWith(tlTask* task, tlArgs* args) {
    tlString* str = tlStringAs(tlArgsTarget(args));
    tlString* start = tlStringCast(tlArgsGet(args, 0));
    if (!start) TL_THROW("arg must be a String");

    int strsize = tlStringSize(str);
    int startsize = tlStringSize(start);
    if (strsize < startsize) return tlFalse;

    int r = strncmp(tlStringData(str) + strsize - startsize, tlStringData(start), startsize);
    return tlBOOL(r == 0);
}

const char* stringtoString(tlHandle v, char* buf, int size) {
    return tlStringData(tlStringAs(v));
}

uint32_t stringHash(tlHandle v) {
    return tlStringHash(tlStringAs(v));
}

static int stringEquals(tlHandle left, tlHandle right) {
    return tlStringEquals(tlStringAs(left), tlStringAs(right));
}

static tlHandle stringCmp(tlHandle left, tlHandle right) {
    return tlCOMPARE(tlStringCmp(tlStringAs(left), tlStringAs(right)));
}

static tlKind _tlStringKind = {
    .name = "String",
    .index = -11,
    .toString = stringtoString,
    .hash = stringHash,
    .equals = stringEquals,
    .cmp = stringCmp,
};

void string_init_first() {
    INIT_KIND(tlStringKind);
}

void string_init() {
    tlClass* cls = tlCLASS("String", null,
    tlMETHODS(
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
        "call", _string_get,
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
    ), tlMETHODS(
        "cat", _String_cat,
        null
    ));
    tlStringKind->cls = cls;
    tl_register_global("String", cls);

    _tl_emptyString = tlSTR("");
}

