#include <regex.h>

#include "trace-off.h"

TL_REF_TYPE(tlRegex);

struct tlRegex {
    tlHead head;
    regex_t compiled;
};
static tlKind _tlRegexKind = { .name = "Regex" };
tlKind* tlRegexKind = &_tlRegexKind;

tlRegex* tlRegexNew(tlText* text, int flags) {
    tlRegex* regex = tlAlloc(tlRegexKind, sizeof(tlRegex));
    int r = regcomp(&regex->compiled, tlTextData(text), flags|REG_EXTENDED);
    if (r) warning("%d", r);
    return regex;
}

static tlHandle _Regex_new(tlArgs* args) {
    tlText* text = tlTextCast(tlArgsGet(args, 0));
    if (!text) TL_THROW("require a pattern");
    int flags = tl_int_or(tlArgsGet(args, 1), 0);
    return tlRegexNew(text, flags);
}

static tlHandle _regex_match(tlArgs* args) {
    tlRegex* regex = tlRegexAs(tlArgsTarget(args));
    tlText* text = tlTextCast(tlArgsGet(args, 0));
    if (!text) TL_THROW("require a text");

    int offset = tl_int_or(tlArgsGet(args, 1), 0);
    if (offset < 0 || offset >= tlTextSize(text)) return tlNull;

    regmatch_t m[1];
    int r = regexec(&regex->compiled, tlTextData(text) + offset, 1, m, offset?REG_NOTBOL:0);
    if (r == REG_NOMATCH) return tlNull;
    if (r == REG_ESPACE) TL_THROW("oeps");

    int b = offset + m[0].rm_so;
    int e = offset + m[0].rm_eo;
    tlText* res = tlTextFromCopy(tlTextData(text) + b, e - b);
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

