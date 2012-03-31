// ** local **

struct tlTagged {
    tlHead head;
    tlTask* task;
    void* tag;
    void* data[];
};

tlTagged* tltagged_new(void* tag, int fields, int privs, tlFreeCb freecb) {
    tlTagged* tagged = tltask_alloc_privs(TLTagged, sizeof(tlTagged), fields, privs, freecb);
    tagged->task = task;
    tagged->tag = tag;
    return tagged;
}

bool tltagged_isvalid(tlTagged* tagged, void* tag) {
    assert(tltagged_is(tagged));
    return tagged->task == task && tagged->tag == tag;
}

void tltagged_priv_set_(tlTagged* tagged, int at, void* data) {
    assert(tltagged_is(tagged));
    assert(at >= 0 && at < tagged->head.size - 2);
    tagged->data[at] = data;
}

void* tltagged_priv(tlTagged* tagged, int at) {
    assert(tltagged_is(tagged));
    assert(at >= 0 && at < tagged->head.size - 2);
    return tagged->data[at];
}

