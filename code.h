#pragma once

bool tcode_is(tValue v);
tCode* tcode_as(tValue v);
tCode* tcode_cast(tValue v);

tCode* tcode_new(tTask* task, int size);
tCode* tcode_from(tTask* task, tList* ops);

void tcode_set_isblock_(tCode* code, bool isblock);
void tcode_set_name_(tCode* code, tSym name);
void tcode_set_args_(tTask* task, tCode* code, tList* args);
void tcode_set_ops_(tCode* code, tList* ops);

bool tcode_isblock(tCode* code);
