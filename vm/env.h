#ifndef _env_h_
#define _env_h_

TL_REF_TYPE(tlEnv);

struct tlEnv {
    tlHead head;
    tlEnv* parent;
    tlArgs* args;
    tlList* names;
    tlHandle data[];
};

tlEnv* tlEnvNew(tlList* names, tlEnv* parent);

tlHandle tlEnvGet(tlEnv* env, int at);
tlHandle tlEnvGetArg(tlEnv* env, int at);
tlEnv* tlEnvGetParentAt(tlEnv* env, int depth);
tlHandle tlEnvSet_(tlEnv* env, int at, tlHandle value);

tlObject* tlEnvLocalObject(tlFrame* frame);

#endif
