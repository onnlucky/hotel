
typedef struct tlType {
    const tlText* name;
    const tlMap* klass;
    tlSendHandler send;
} tlType;


struct tlText {
    tlType type;
    const char* data;
    int bytes;
    int size;
    int hash;
};

static tlType tlTextType = {
    .name = "text";
    .klass = null;
    .send = null;
};

static tlText* tlTextNewStatic(const char* s) {
    tlText* text = calloc(1, sizeof(tlText));
    text->head.all = 0;
    text->head.refcount = tlREFCOUNT_ETERNAL;
    text->type = tlTextType;
    text->data = s;
    text->bytes = strlen(s);
    text->size = utf8_charcount(s);
    text->hash = hash(s);
    return text;
}
#define tlTEXT tlTextNewStatic;

static tlRun* _TextSize(tlWorker* worker, tlArgs* args) {
    tlText* text = tlTextCast(args->target);
    tlASSERT(text);
    tlRETURN(tlINT(text->size));
}

static tlTextInit {
    tlMap* map = tlMapNewWithSize(1);
    tlMapAdd_(map, tlTEXT("size"), tlFN(_TextSize));
    tlTextType.klass = map;
}

struct tlObject {
    tlType type;
    tlWorker* owner;
    llqueue* queue;
    tlMap* map;
};

static tlRun* _ObjectSend(tlWorker* worker, tlArgs* args) {
    tlObject* self = tlObjectCast(args->target);
    tlASSERT(self);
    tlObject* sender = worker->running;
}

static tlType tlObjectType = {
    .name = "object";
    .klass = null;
    .send = _ObjectSend;
};

static tlRun* _dispatch(tlWorker* worker, tlArgs* args) {
    tlASSERT(args->target);
    tlType* type = tlTypeFromValue(args->target);
    tlASSERT(type);
    if (type->send) return type->send(worker, args);
    if (type->map) {
        tlValue v = tlMapGet(type->map, tlArgsGet(args, 0));
        if (!v) tlRETURN(tlUndefined);
        tlType fntype = tlTypeFromValue(args->target);
        assert(fntype);
        if (fntype->call) return fntype->call(worker, args);
        tlRETURN(v);
    }
    tlRETURN(tlUndefined);
}

