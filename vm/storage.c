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

tlStorage* tlStorageNew(tlText* text) {
    const char* name = 0;
    if (text) name = tlTextData(text);
    DB* db = dbopen(name, O_CREAT|O_RDWR, 0666, DB_BTREE, 0);
    if (!db) TL_THROW("unable to open storage '%s': %s", name, strerror(errno));

    tlStorage* storage = tlAlloc(tlStorageKind, sizeof(tlStorage));
    storage->db = db;

    return storage;
}
INTERNAL tlHandle _storage_get(tlArgs* args) {
    tlStorage* storage = tlStorageAs(tlArgsTarget(args));
    tlText* tkey = tlTextCast(tlArgsGet(args, 0));
    if (!tkey) TL_THROW("get requires a Text key");

    DBT key = { (char*)tlTextData(tkey), tlTextSize(tkey) + 1 };
    DBT val;
    int r = storage->db->get(storage->db, &key, &val, 0);
    storage->db->sync(storage->db, 0);
    if (r == -1) TL_THROW("StoreError: %s", strerror(errno));
    if (r == 0) return tlTextFromCopy(val.data, val.size - 1);
    return tlNull;
}
INTERNAL tlHandle _storage_set(tlArgs* args) {
    tlStorage* storage = tlStorageAs(tlArgsTarget(args));
    tlText* tkey = tlTextCast(tlArgsGet(args, 0));
    if (!tkey) TL_THROW("set requires a Text key");
    tlText* tval = tlTextCast(tlArgsGet(args, 1));
    if (!tval) TL_THROW("set requires a Text value");

    DBT key = { (char*)tlTextData(tkey), tlTextSize(tkey) + 1 };
    DBT val = { (char*)tlTextData(tval), tlTextSize(tval) + 1 };
    int r = storage->db->put(storage->db, &key, &val, 0);
    storage->db->sync(storage->db, 0);
    if (r == -1) TL_THROW("StoreError: %s", strerror(errno));
    return tval;
}
INTERNAL tlHandle _storage_del(tlArgs* args) {
    tlStorage* storage = tlStorageAs(tlArgsTarget(args));
    tlText* tkey = tlTextCast(tlArgsGet(args, 0));
    if (!tkey) TL_THROW("del requires a Text key");

    DBT key = { (char*)tlTextData(tkey), tlTextSize(tkey) + 1 };
    int r = storage->db->del(storage->db, &key, 0);
    storage->db->sync(storage->db, 0);
    if (r == -1) TL_THROW("StoreError: %s", strerror(errno));
    return tlNull;
}

INTERNAL tlHandle _Storage_new(tlArgs* args) {
    tlHandle h = tlArgsGet(args, 0);
    if (h && !tlTextIs(h)) TL_THROW("first argument must be a file name or null for memory store");
    return tlStorageNew(tlTextAs(h));
}

void storage_init() {
    _tlStorageKind.klass = tlClassMapFrom(
        "get", _storage_get,
        "set", _storage_set,
        "del", _storage_del,
        null
    );
}

static void storage_init_vm(tlVm* vm) {
    tlMap* StorageStatic = tlClassMapFrom(
        "new", _Storage_new,
        null
    );
    tlVmGlobalSet(vm, tlSYM("Storage"), StorageStatic);
}

