#pragma once

bool tbody_is(tValue v);
tBody* tbody_as(tValue v);
tBody* tbody_cast(tValue v);

tBody* tbody_new(tTask* task, int size);
tBody* tbody_from(tTask* task, tList* ops);

void tbody_set_name_(tBody* body, tSym name);
void tbody_set_args_(tTask* task, tBody* body, tList* args);
void tbody_set_ops_(tBody* body, tList* ops);

