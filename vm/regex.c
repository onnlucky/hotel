#include <regex.h>

#include "trace-off.h"

TL_REF_TYPE(tlRegex);

struct tlRegex {
    tlHead head;
    regex_t compiled;
};
static tlKind _tlRegexKind = { .name = "Regex" };
tlKind* tlRegexKind = &_tlRegexKind;

static tlHandle _Regex_new(tlArgs* args) {
    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("require a pattern");
    int flags = tl_int_or(tlArgsGet(args, 1), 0);

    tlRegex* regex = tlAlloc(tlRegexKind, sizeof(tlRegex));
    int r = regcomp(&regex->compiled, tlStringData(str), flags|REG_EXTENDED);
    if (r) {
        char buf[1024];
        regerror(r, &regex->compiled, buf, sizeof(buf));
        TL_THROW("Regex: %s", buf);
    }
    return regex;
}

static tlHandle _regex_match(tlArgs* args) {
    tlRegex* regex = tlRegexAs(tlArgsTarget(args));
    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("require a String");

    int offset = tl_int_or(tlArgsGet(args, 1), 0);
    if (offset < 0 || offset >= tlStringSize(str)) return tlNull;

    regmatch_t m[3];
    int r = regexec(&regex->compiled, tlStringData(str) + offset, 3, m, offset?REG_NOTBOL:0);
    if (r == REG_NOMATCH) return tlNull;
    if (r == REG_ESPACE) TL_THROW("oeps");

    int b = offset + m[0].rm_so;
    int e = offset + m[0].rm_eo;
    if (m[1].rm_so != -1 && m[2].rm_so == -1) {
        b = offset + m[1].rm_so;
        e = offset + m[1].rm_eo;
    }
    tlString* res = tlStringFromCopy(tlStringData(str) + b, e - b);
    return tlResultFrom(res, tlINT(b), tlINT(e), null);
}

void regex_init_vm(tlVm* vm) {
    if (!_tlRegexKind.klass) {
        _tlRegexKind.klass = tlClassMapFrom(
            "match", _regex_match,
            null
        );
    }
    tlMap* RegexStatic = tlClassMapFrom(
        "call", _Regex_new,
        null
    );
    tlVmGlobalSet(vm, tlSYM("Regex"), RegexStatic);
}

