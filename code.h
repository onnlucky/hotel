#pragma once

bool tlActiveIs(tlValue v);
tlValue tl_active(tlValue value);
tlValue tl_value(tlValue active);

tlCode* tlCodeNew(tlTask* task, int size);
tlCode* tlCodeFrom(tlTask* task, tlList* ops);

void tlCodeSetIsBlock_(tlCode* code, bool isblock);
bool tlCodeIsBlock(tlCode* code);

void tlCodeSetName_(tlCode* code, tlSym name);
void tlCodeSetArgs_(tlTask* task, tlCode* code, tlList* args);
void tlCodeSetOps_(tlCode* code, tlList* ops);

tlCall* tlCallNew(tlTask* task, int argc, bool keys);
tlCall* tlCallFrom(tlTask* task, ...);
tlCall* tlCallFromList(tlTask* task, tlValue fn, tlList* args);
tlCall* tlCallSendFromList(tlTask* task, tlValue fn, tlValue oop, tlValue msg, tlList* args);
tlCall* tlCallAddBlock(tlTask* task, tlValue call, tlCode* block);

tlValue tlCallValueIter(tlCall* call, int i);
tlCall* tlCallValueIterSet_(tlCall* call, int i, tlValue v);

tlValue tlCallGetFn(tlCall* call);
void tlCallSetFn_(tlCall* call, tlValue v);

tlValue tlCallGet(tlCall* call, int at);
void tlCallSet_(tlCall* call, int at, tlValue v);

tlCollect* tlCollectFromList_(tlList* list);

void debugcode(tlCode* code);
