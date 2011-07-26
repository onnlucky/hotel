#pragma once

bool tlcode_is(tlValue v);
tlCode* tlcode_as(tlValue v);
tlCode* tlcode_cast(tlValue v);

tlCode* tlcode_new(tlTask* task, int size);
tlCode* tlcode_from(tlTask* task, tlList* ops);

void tlcode_set_isblock_(tlCode* code, bool isblock);
void tlcode_set_name_(tlCode* code, tlSym name);
void tlcode_set_args_(tlTask* task, tlCode* code, tlList* args);
void tlcode_set_ops_(tlCode* code, tlList* ops);

bool tlcode_isblock(tlCode* code);

tlCall* tlcall_new(tlTask* task, int argc, bool keys);
tlCall* tlcall_from_list(tlTask* task, tlValue fn, tlList* args);
tlCall* tlcall_send_from_list(tlTask* task, tlValue fn, tlValue oop, tlValue msg, tlList* args);
void tlcall_fn_set_(tlCall* call, tlValue fn);
void tlcall_arg_set_(tlCall* call, int at, tlValue v);

