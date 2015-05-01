#ifndef _object_h_
#define _object_h_

#include "tl.h"

struct tlObject {
    tlHead head;
    tlSet* keys;
    tlHandle data[];
};

tlHandle tlObjectValueIter(const tlObject* object, int i);
tlHandle tlObjectKeyIter(const tlObject* object, int i);
bool tlObjectKeyValueIter(const tlObject* object, int i, tlHandle* keyp, tlHandle* valuep);

struct tlClass {
    tlHead head;
    tlSym name;
    tlSet* fields;
    tlClass* super;
    tlList* mixins; // a list of tlClasses or objects, while they can extend the functionality, they cannot add state (fields)
    tlObject* methods;
    tlObject* statics;
    tlHandle constructor; // when called as Example()
};

tlClass* tlClassFor(tlHandle value);

TL_REF_TYPE(tlUserClass);

//TL_REF_TYPE(tlUserObject); // cannot use this, as we want both mutable and immutable to work
typedef struct tlUserObject tlUserObject;
extern tlKind* tlUserObjectKind;
extern tlKind* tlMutableUserObjectKind;

static inline bool tlUserObjectIs(const tlHandle v) {
    return tl_kind(v) == tlUserObjectKind || tl_kind(v) == tlMutableUserObjectKind;
}
static inline tlUserObject* tlUserObjectAs(tlHandle v) {
    assert(tlUserObjectIs(v) || !v); return (tlUserObject*)v;
}
static inline tlUserObject* tlUserObjectCast(tlHandle v) {
    return tlUserObjectIs(v)?(tlUserObject*)v:null;
}

struct tlUserClass {
    tlHead head;
    tlSym name;
    bool mutable;
    tlHandle constructor;
    tlList* extends;
    tlSet* fields;
    tlObject* methods;
    tlObject* classfields;
};

// for now, all user objects have locks, even though there are two kinds, mutable and immutable
struct tlUserObject {
    tlLock lock;
    tlUserClass* cls;
    tlHandle fields[];
};

tlUserClass* tlUserClassFor(tlUserObject* oop);

tlHandle userobjectResolve(tlUserObject* oop, tlSym name);
tlHandle userclassResolveStatic(tlUserClass* cls, tlSym name);


uint32_t tlObjectHash(tlObject* object, tlHandle* unhashable);

tlHandle classResolveStatic(tlClass* cls, tlSym name);
tlHandle classResolve(tlClass* cls, tlSym name);
tlHandle objectResolve(tlObject* object, tlSym msg);

void class_init_first();
void object_init();
void class_init();

#endif
