#include <regex.h>

#include "trace-off.h"

TL_REF_TYPE(tlRegex);
struct tlRegex {
    tlHead head;
    regex_t compiled;
};
static tlKind _tlRegexKind = { .name = "Regex" };
tlKind* tlRegexKind = &_tlRegexKind;

TL_REF_TYPE(tlMatch);
struct tlMatch {
    tlHead head;
    tlString* string;
    int size;
    regmatch_t groups[];
};
static tlKind _tlMatchKind = { .name = "Match" };
tlKind* tlMatchKind = &_tlMatchKind;

/// object Regex: create posix compatible regular expressions to match strings

static void free_regex(void* _regex, void* data) {
    tlRegex* regex = tlRegexAs(_regex);
    regfree(&regex->compiled);
}

/// call(pattern): create a regex using #pattern
/// > Regex("[^ ]+")
// TODO expose flags
static tlHandle _Regex_new(tlArgs* args) {
    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("require a pattern");
    int flags = tl_int_or(tlArgsGet(args, 1), 0);

    tlRegex* regex = tlAlloc(tlRegexKind, sizeof(tlRegex));
    int r = regcomp(&regex->compiled, tlStringData(str), flags|REG_EXTENDED);
#ifdef HAVE_BOEHMGC
    GC_REGISTER_FINALIZER_NO_ORDER(regex, free_regex, null, null, null);
#endif
    if (r) {
        char buf[1024];
        regerror(r, &regex->compiled, buf, sizeof(buf));
        TL_THROW("Regex: %s", buf);
    }
    return regex;
}

/// find(input): match a regex against #input and return the begin position and end position of the pattern, or null if not found
/// > s, e = Regex("[^ ]+").find("test more")
/// > assert s == 1 and e == 4
// TODO expose flags
static tlHandle _regex_find(tlArgs* args) {
    tlRegex* regex = tlRegexAs(tlArgsTarget(args));
    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("require a String");

    int offset = tl_int_or(tlArgsGet(args, 1), 0);
    if (offset < 0 || offset >= tlStringSize(str)) return tlNull;

    int msize = 1;
    regmatch_t m[msize];
    int r = regexec(&regex->compiled, tlStringData(str) + offset, msize, m, offset?REG_NOTBOL:0);
    if (r == REG_NOMATCH) return tlNull;
    if (r == REG_ESPACE) TL_THROW("out of memory");
    return tlResultFrom(tlINT(m[0].rm_so + 1), tlINT(m[0].rm_eo), null);
}

/// match(string): match a regex against an input returns a #Match object or null if not a match
/// > Regex("[^ ]+").match("test more").main == "test"
// TODO expose flags
static tlHandle _regex_match(tlArgs* args) {
    tlRegex* regex = tlRegexAs(tlArgsTarget(args));
    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("require a String");

    int offset = tl_int_or(tlArgsGet(args, 1), 0);
    if (offset < 0 || offset >= tlStringSize(str)) return tlNull;

    int msize = 256;
    regmatch_t m[msize];
    int r = regexec(&regex->compiled, tlStringData(str) + offset, msize, m, offset?REG_NOTBOL:0);
    if (r == REG_NOMATCH) return tlNull;
    if (r == REG_ESPACE) TL_THROW("out of memory");

    int size = 0;
    for (size = 0; size < msize; size++) {
        if (m[size].rm_so == -1) break;
    }
    tlMatch* match = tlAlloc(tlMatchKind, sizeof(tlMatch) + sizeof(regmatch_t) * size);
    match->string = str;
    match->size = size - 1;
    for (int i = 0; i < size; i++) {
        match->groups[i] = m[i];
    }
    return match;
}

/// object Match: a #Regex result

/// main: the main result of the match, the part of the input string that actually matched the #Regex
static tlHandle _match_main(tlArgs* args) {
    tlMatch* match = tlMatchAs(tlArgsTarget(args));
    return tlStringFromCopy(tlStringData(match->string) + match->groups[0].rm_so, match->groups[0].rm_eo - match->groups[0].rm_so);
}

/// size: the amount of regex subexpressions matched
static tlHandle _match_size(tlArgs* args) {
    tlMatch* match = tlMatchAs(tlArgsTarget(args));
    return tlINT(match->size);
}

/// get(at): get a subexpression
/// > m = Regex("\\[([^]]+)\\]\s*(.*)").match("[2014-01-01] hello world")
/// > m[1] == "2014-01-01"
/// > m[2] == "hello world"
static tlHandle _match_get(tlArgs* args) {
    tlMatch* match = tlMatchAs(tlArgsTarget(args));
    int at = at_offset(tlArgsGet(args, 0), match->size);
    if (at < 0) return tlUndef();
    at++;
    return tlStringFromCopy(tlStringData(match->string) + match->groups[at].rm_so, match->groups[at].rm_eo - match->groups[at].rm_so);
}

/// group(at): get begin and end of a subexpression
/// > m = Regex("\\[([^]]+)\\]\s*(.*)").match("[2014-01-01] hello world")
/// > b, e = m.group(1)
/// > assert b == 2 and e == 11
static tlHandle _match_group(tlArgs* args) {
    tlMatch* match = tlMatchAs(tlArgsTarget(args));
    int at = at_offset(tlArgsGet(args, 0), match->size);
    if (at < 0) return tlUndef();
    at++;
    return tlResultFrom(tlINT(match->groups[at].rm_so + 1), tlINT(match->groups[at].rm_eo), null);
}

static tlHandle _isRegex(tlArgs* args) { return tlBOOL(tlRegexIs(tlArgsGet(args, 0))); }

void regex_init_vm(tlVm* vm) {
    if (!_tlRegexKind.klass) {
        _tlRegexKind.klass = tlClassObjectFrom(
            "find", _regex_find,
            "match", _regex_match,
            null
        );
    }
    tlObject* RegexStatic = tlClassObjectFrom(
        "call", _Regex_new,
        null
    );
    tlVmGlobalSet(vm, tlSYM("Regex"), RegexStatic);

    if (!_tlMatchKind.klass) {
        _tlMatchKind.klass = tlClassObjectFrom(
            "main", _match_main,
            "size", _match_size,
            "get", _match_get,
            "group", _match_group,
            null
        );
    }
}

