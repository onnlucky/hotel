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
TL_REF_TYPE(tlUserObject);
struct tlUserClass {
    tlHead head;
    tlSym name;
    tlHandle constructor;
    tlList* inherits;
    tlList* fields;
    tlObject* methods;
    tlObject* classfields;
};

struct tlUserObject {
    tlHead head;
    tlUserClass* cls;
    tlHandle fields[];
};

tlHandle userobjectResolve(tlUserObject* oop, tlSym name);


uint32_t tlObjectHash(tlObject* object, tlHandle* unhashable);

tlHandle classResolveStatic(tlClass* cls, tlSym name);
tlHandle classResolve(tlClass* cls, tlSym name);
tlHandle objectResolve(tlObject* object, tlSym msg);

void class_init_first();
void object_init();
void class_init();

#endif
