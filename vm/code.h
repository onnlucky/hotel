#pragma once

bool tlActiveIs(tlHandle v);
tlHandle tl_active(tlHandle value);
tlHandle tl_value(tlHandle active);

tlCode* tlCodeNew(int size);
tlCode* tlCodeFrom(tlList* ops, tlString* file, tlInt line);

void tlCodeSetIsBlock_(tlCode* code, bool isblock);
bool tlCodeIsBlock(tlCode* code);

void tlCodeSetInfo_(tlCode* code, tlString* file, tlInt line, tlSym name);
void tlCodeSetArgs_(tlCode* code, tlList* args);
void tlCodeSetOps_(tlCode* code, tlList* ops);

tlBCall* tlBCallFrom(tlHandle v1, ...);
tlBCall* tlBCallFromList(tlHandle fn, tlList* args);

//tlCall* tlCallNew(int argc, bool keys);
//tlCall* tlCallSendFromList(tlHandle fn, tlHandle oop, tlHandle msg, tlList* args);
//tlCall* tlCallAddBlock(tlHandle call, tlCode* block);

//tlHandle tlCallValueIter(tlCall* call, int i);
//tlCall* tlCallValueIterSet_(tlCall* call, int i, tlHandle v);

//tlHandle tlCallGetFn(tlCall* call);
//void tlCallSetFn_(tlCall* call, tlHandle v);

//tlHandle tlCallGet(tlCall* call, int at);
//void tlCallSet_(tlCall* call, int at, tlHandle v);

tlCollect* tlCollectFromList_(tlList* list);

void debugcode(tlCode* code);

