#include "platform.h"

#include "value.c"
#include "text.c"
#include "list.c"

#include "eval.c"

#include "buffer.c"

#include "compile.c"
#include "grammar.c"

const size_t type_to_size[] = {
    -1,
    -1, -1, -1, -1, -1, -1/*sizeof(tFloat)*/,
    sizeof(tList), -1/*sizeof(tMap)*/, sizeof(tEnv),
    sizeof(tText), sizeof(tMem),
    sizeof(tCall), sizeof(tThunk), sizeof(tResult),
    sizeof(tFun), sizeof(tCFun),
    sizeof(tCode), sizeof(tFrame), sizeof(tArgs), sizeof(tEvalFrame),
    sizeof(tTask), -1, -1 /*sizeof(tVar), sizeof(tCTask)*/
};

int main() {
    parse(tTEXT("print(43)"));
    print("done");
}
