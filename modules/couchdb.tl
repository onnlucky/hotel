# a simple couchdb implementation
# ```
# db = couchdb.open("dbname", "localhost", 5984)
# doc = db.get("mydata", create={exists=true,seen=0})
# assert doc.exists
# doc.seen += 1
# print doc.seen
# ```

DEBUG = false

query = base, url, data ->
    url = base + url
    body = { data }: json.toString(data)
           {      }: null
    method = args["method"] or "GET"
    if DEBUG: print ">", method, url, try(body[:50])
    data, res = http.send(method, url, body, contentType=args["contentType"], accept="application/json")
    if DEBUG: print "<", res.status, repr(data)
    if res.status >= 300: return null, data, res.status
    return data

# using a weak map, so we only ever have a single Document alive
docs = WeakMap.new
outs = IdentitySet.new
changes = Array.new

store = doc ->
    res = query(doc._db.base, "/", doc._back, method="POST", contentType="application/json")
    id = res.id
    assert id
    docs[res.id] = doc
    doc._back["_id"] = id
    doc._back["_rev"] = res.rev

create = db, key, data ->
    res, err = query(db.base, "/$key", data, method="PUT", contentType="application/json")
    if err: return null
    id = res.id
    assert id == key
    docs[key] = Document(db, Object.set(data, {_id=key,_rev=res.rev}))

save = doc ->
    id = doc._id
    rev = doc._rev
    assert id, docs[id]
    res = query(doc._db.base, "/$id?rev=$rev", doc._back, method="PUT")
    doc._back["_rev"] = res.rev

remove = doc ->
    id = doc._id
    rev = doc._rev
    res = query(doc._db.base, "/$id?rev=$rev", method="DELETE")
    # TODO check result
    docs.del(id)
    doc._back.clear

DocumentClass = {
    _get = -> this._back.get(args.msg)
    _set = k, v ->
        if k == "_id" or k == "_rev": return v
        this._back.set(k, v)
        { _has_lock(changes) }: outs.add(this)
        {                    }: save(this)
        return v
}
Document = db, data ->
    id = data?_id
    if id: doc = docs[id]; if doc: return doc

    doc = {
        class = DocumentClass
        _db = db
        _back = HashMap.new(data)
    }
    { id                 }: docs[id] = doc
    { _has_lock(changes) }: outs.add(doc)
    {                    }: store(doc)
    return doc

sync = db, aftercb ->
    _with_lock(changes):
        outs.each: doc, n ->
            { doc._id }: save(doc)
            {         }: store(doc)
        outs.clear
        changes.each: change ->
            id = change.id
            rev = change.changes.first?rev
            doc = docs[id]
            if DEBUG: print "process changes>", doc?_rev, rev, doc?_rev == rev
            if not doc: continue
            if doc._rev == rev: continue
            # TODO older??
            data = query(db.base, "/$(change.id)")
            if DEBUG: print "apply changes>", repr(data)
            doc._back.setAll(data)
        if DEBUG: print "SYNCED", changes.size, aftercb
        if changes.size > 0 and aftercb: aftercb()
        changes.clear

Database = dbname, host, port ->
    host = host or "localhost"
    port = Number(port) or 5984
    base = "http://$host:$port/$dbname"
    reply, err = query(base, "/", method="PUT")
    { reply?ok                     }: null
    { err?error == "file_exists"   }: null
    {                              }: Error(repr(reply or err)).throw

    var afterChangeHandler
    var synctask
    checkopen = -> if not synctask: Error("database is closed").throw
    db = {
        host = host
        port = port
        dbname = dbname
        base = base
        onchange = fn -> afterChangeHandler = args.block or fn
        new = data -> Document(this, data)
        size = -> query(base, "/_all_docs?limit=0").total_rows
        get = key ->
            checkopen()
            data, err = query(base, "/$key")
            { data                     }: Document(this, data)
            { isObject(args["create"]) }: create(this, key, args["create"])
            { args["create"]           }: create(this, key, {})
            {                          }: null
        del = doc ->
            checkopen()
            { isString(doc)   }: query(base, "/$doc", method="DELETE")
            { isDocument(doc) }: remove(doc)
            {                 }: Error("not a document or key: '$doc'").throw
            return
        each = fn ->
            checkopen()
            block = args.block or fn
            if not block: Error("each requires a block or function").throw
            data = query(base, "/_all_docs?include_docs=true")
            var n = 0
            loop:
                n += 1
                row = data.rows[n]
                if not row: return
                block(Document(this, row.doc), n)
        toList = ->
            checkopen()
            as = Array.new
            this.each: d -> as.add(d)
            return as.toList
        with = fn ->
            checkopen()
            block = args.block or fn
            if not block: Error("with requires block or function").throw
            _with_lock(changes):
                block()
                sync(this)
        close = ->
            if not synctask: return
            synctask.abort
            synctask = null
        drop = ->
            checkopen()
            reply = query(base, "/", method="DELETE")
            this.close
    }

    synctask = !(
        catch: e -> print "changes>", toString(e)
        Task.current.background(true)
        stream, res = http.send("GET", "$(base)/_changes?feed=continuous&heartbeat=true&since=now", stream=true, accept="application/json")
        loop:
            line = stream.find("\n")
            line = stream.readString(line)
            if line.size <= 1: continue
            data = json.parse(line)
            if DEBUG: print "changes>", repr(data)
            changes.add(data)
            !sync(db, afterChangeHandler)
    )
    return db

open = name, host, port -> Database(name, host, port)
{{ open }}
