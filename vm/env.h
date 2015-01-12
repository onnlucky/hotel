#ifndef _env_h_
#define _env_h_

TL_REF_TYPE(tlEnv);

struct tlEnv {
    tlHead head;
    tlEnv* parent; // scopes go down, though though stopping at the module level
    tlArgs* args;
    tlEnv* link;   // for repl and such, envs can be linked so mutable variables can be closed over
    tlObject* imports;
    tlList* names;
    tlHandle data[];
};

tlEnv* tlEnvNew(tlList* names, tlEnv* parent);
int tlEnvSize(tlEnv* env);

void tlEnvLink_(tlEnv* env, tlEnv* link, tlList* localvars);
void tlEnvImport_(tlEnv* env, tlObject* names);

tlHandle tlEnvGetName(tlEnv* env, tlSym name, int depth);
tlHandle tlEnvGetFull(tlEnv* env, int depth, int at);

tlHandle tlEnvGet(tlEnv* env, int at);
tlHandle tlEnvGetArg(tlEnv* env, int at);
tlEnv* tlEnvGetParentAt(tlEnv* env, int depth);
tlHandle tlEnvSet_(tlEnv* env, int at, tlHandle value);
tlHandle tlEnvGetVar(tlEnv* env, int at);
tlHandle tlEnvSetVar_(tlEnv* env, int at, tlHandle value);

tlObject* tlEnvLocalObject(tlFrame* frame);

#endif
