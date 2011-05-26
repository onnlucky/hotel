#pragma once

bool tbody_is(tValue v);
tBody* tbody_as(tValue v);
tBody* tbody_cast(tValue v);

tBody* tbody_new(tTask* task, int size);
tBody* tbody_from(tTask* task, tList* ops);

void tbody_set_name_(tBody* body, tSym name);
void tbody_set_argnames_(tBody* body, tList* names);
void tbody_set_argdefaults_(tBody* body, tList* defaults);
void tbody_set_ops_(tBody* body, tList* ops);

void tbody_set_arg_name_defaults_(tTask* task, tBody* body, tList* name_defaults);

