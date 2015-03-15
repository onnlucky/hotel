#if 0
#include <db.h>

// TODO properly close the db, and sync it every 500 ms after last mutation or so?
// TODO use json or serialize binary to properly store any tl value ...

TL_REF_TYPE(tlStorage);
struct tlStorage {
    tlLock lock;
    DB* db;
};
static tlKind _tlStorageKind = {
    .name = "Storage",
    .locked = true,
};
tlKind* tlStorageKind = &_tlStorageKind;

// encoding:
// string: data = "string\0", size = 5 && data[size - 1] == 0
// int: data = <4 bytes; 1>, size = 5 && data[size - 1] == 1
// float: data = <8 bytes; 1>, size = 9 && data[size - 1] == 1
// null/false/true = <1/2/3>, size = 1 && data[size - 1] != 0

static tlHandle decode(DBT* d);
static int encode(tlHandle h, DBT* d, uint8_t* buf) {
    // TODO int and double are machine endianess dependent ...
    if (tlStringIs(h)) {
        tlString* t = tlStringAs(h);
        d->data = (char*)tlStringData(t);
        d->size = tlStringSize(t) + 1;
    } else if (tlIntIs(h)) {
        *((int*)buf) = tl_int(h);
        buf[4] = 1;
        d->data = buf;
        d->size = 5;
    } else if (tlFloatIs(h)) {
        *((double*)buf) = tl_double(h);
        buf[8] = 1;
        d->data = buf;
        d->size = 9;
    } else if (h == tlNull) {
        buf[0] = 1;
        d->data = buf;
        d->size = 1;
    } else if (h == tlFalse) {
        buf[0] = 2;
        d->data = buf;
        d->size = 1;
    } else if (h == tlTrue) {
        buf[0] = 3;
        d->data = buf;
        d->size = 1;
    } else {
        return -1;
    }
    assert(tlHandleEquals(decode(d), h));
    return 0;
}

tlHandle decode(DBT* d) {
    uint8_t* buf = (uint8_t*)d->data;
    if (d->size == 1) {
        if (buf[0] == 0) return tlStringEmpty();
        if (buf[0] == 1) return tlNull;
        if (buf[0] == 2) return tlFalse;
        if (buf[0] == 3) return tlTrue;
        assert(false);
    }
    if (d->size == 5 && buf[4] == 1) {
        return tlINT(*((int*)buf));
    }
    if (d->size == 9 && buf[8] == 1) {
        return tlFLOAT(*((double*)buf));
    }
    assert(buf[d->size - 1] == 0);
    return tlStringFromCopy((char*)buf, d->size - 1);
}

tlStorage* tlStorageNew(tlString* str) {
    const char* name = 0;
    if (str) name = tlStringData(str);
    DB* db = dbopen(name, O_CREAT|O_RDWR, 0666, DB_BTREE, 0);
    if (!db) TL_THROW("unable to open storage '%s': %s", name, strerror(errno));

    tlStorage* storage = tlAlloc(tlStorageKind, sizeof(tlStorage));
    storage->db = db;

    return storage;
}
static tlHandle _storage_get(tlArgs* args) {
    tlStorage* storage = tlStorageAs(tlArgsTarget(args));
    int r;

    DBT key;
    uint8_t kbuf[10];
    r = encode(tlArgsGet(args, 0), &key, kbuf);
    if (r) TL_THROW("cannot encode key");

    DBT val;

    r = storage->db->get(storage->db, &key, &val, 0);
    storage->db->sync(storage->db, 0);
    if (r == -1) TL_THROW("StoreError: %s", strerror(errno));
    if (r == 0) return decode(&val);
    return tlNull;
}
static tlHandle _storage_set(tlArgs* args) {
    tlStorage* storage = tlStorageAs(tlArgsTarget(args));
    int r;

    DBT key;
    uint8_t kbuf[10];
    r = encode(tlArgsGet(args, 0), &key, kbuf);
    if (r) TL_THROW("cannot encode key");

    DBT val;
    uint8_t vbuf[10];
    r = encode(tlArgsGet(args, 1), &val, vbuf);
    if (r) TL_THROW("cannot encode value");

    r = storage->db->put(storage->db, &key, &val, 0);
    storage->db->sync(storage->db, 0);
    if (r == -1) TL_THROW("StoreError: %s", strerror(errno));
    return tlArgsGet(args, 1);
}
static tlHandle _storage_del(tlArgs* args) {
    tlStorage* storage = tlStorageAs(tlArgsTarget(args));
    int r;

    DBT key;
    uint8_t kbuf[10];
    r = encode(tlArgsGet(args, 0), &key, kbuf);
    if (r) TL_THROW("cannot encode key");

    r = storage->db->del(storage->db, &key, 0);
    storage->db->sync(storage->db, 0);
    if (r == -1) TL_THROW("StoreError: %s", strerror(errno));
    return tlNull;
}

static tlHandle _Storage_new(tlArgs* args) {
    tlHandle h = tlArgsGet(args, 0);
    if (h && !tlStringIs(h)) TL_THROW("first argument must be a file name or null for memory store");
    return tlStorageNew(tlStringAs(h));
}

void storage_init() {
    _tlStorageKind.klass = tlClassObjectFrom(
        "get", _storage_get,
        "set", _storage_set,
        "del", _storage_del,
        null
    );
}

static void storage_init_vm(tlVm* vm) {
    tlObject* StorageStatic = tlClassObjectFrom(
        "new", _Storage_new,
        null
    );
    tlVmGlobalSet(vm, tlSYM("Storage"), StorageStatic);
}
#endif

