#pragma once

bool tlActiveIs(tlValue v);
tlValue tl_active(tlValue value);
tlValue tl_value(tlValue active);

tlCode* tlCodeNew(int size);
tlCode* tlCodeFrom(tlList* ops, tlText* file, tlInt line);

void tlCodeSetIsBlock_(tlCode* code, bool isblock);
bool tlCodeIsBlock(tlCode* code);

void tlCodeSetInfo_(tlCode* code, tlText* file, tlInt line, tlSym name);
void tlCodeSetArgs_(tlCode* code, tlList* args);
void tlCodeSetOps_(tlCode* code, tlList* ops);

tlCall* tlCallNew(int argc, bool keys);
tlCall* tlCallFrom(tlValue v1, ...);
tlCall* tlCallFromList(tlValue fn, tlList* args);
tlCall* tlCallSendFromList(tlValue fn, tlValue oop, tlValue msg, tlList* args);
tlCall* tlCallAddBlock(tlValue call, tlCode* block);

tlValue tlCallValueIter(tlCall* call, int i);
tlCall* tlCallValueIterSet_(tlCall* call, int i, tlValue v);

tlValue tlCallGetFn(tlCall* call);
void tlCallSetFn_(tlCall* call, tlValue v);

tlValue tlCallGet(tlCall* call, int at);
void tlCallSet_(tlCall* call, int at, tlValue v);

tlCollect* tlCollectFromList_(tlList* list);

void debugcode(tlCode* code);

