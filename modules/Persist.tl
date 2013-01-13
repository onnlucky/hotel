new = name ->
    fd = io.File(name).open(create=true)
    ids = HashMap.new

    record = cmd ->
        buf = _to_repr(cmd); buf.write("\n"); fd.write(buf)

    PersistObjectClass = {
        _name: #PersistObjectClass
        _get: -> this._back.get(args.msg)
        _set: k, v -> this._back.set(k, v); record {set:true,id:this._id,k:k,v:v}; return v
    }
    PersistListClass = {
        _name: #PersistListClass
        add: v -> i = this._back.add(v); record {set:true,id:this._id,k:i,v:v}; return i
        set: i, v -> this._back.set(i, v); record {set:true,id:this._id,k:i,v:v}; return v
        remove: at -> r = this._back.remove(at); record {del:true,id:this._id,k:at}; return r
        get: i -> this._back.get(i)
        // notice these are "delegates" to the _black-ing store
        size: -> this._back.size
        each: fn -> this._back.each(args.block or fn)
        find: fn -> this._back.find(args.block or fn)
        map: fn -> this._back.map(args.block or fn)
    }

    genid = -> loop: id = random(); if not ids.has(id): return id
    object_new = id, silent ->
        id = id or genid()
        if not silent: record {new:#Object,id:id}
        res = {_id: id, _back: HashMap.new, class: PersistObjectClass}
        ids.set(id, res)
    list_new = id, silent ->
        id = id or genid()
        if not silent: record {new:#List,id:id}
        res = {_id: id, _back: Array.new, class: PersistListClass}
        ids.set(id, res)
    revive = v ->
        if isObject(v):
            if v?class?_name == #PersistObjectClass: return ids[v._id]
            if v?class?_name == #PersistListClass: return ids[v._id]
        return v
    replay = cmd ->
        if cmd?new == #Object: return object_new(cmd.id, true)
        if cmd?new == #List: return list_new(cmd.id, true)
        if cmd?set: return ids[cmd.id]._back.set(cmd.k, revive(cmd.v))
        if cmd?del: return ids[cmd.id]._back.remove(cmd.k)
        assert(false, "unknown command: $(cmd)")

    // so we don't leak a buffer
    (
        buf = Buffer.new
        loop:
            fd.read buf
            if buf.size == 0: break
            while buf.find("\n"):
                cmd = _from_repr(buf)
                assert buf.read(1) == "\n"
                replay(cmd)
    )

    return {
        Object: {
            new: (from ->
                object = object_new()
                if from: _Map_keys(from).each: k -> object._set(k, _Map_get(from, k))
                return object
            )
            get: (name ->
                object = ids[name]
                if object:
                    if object.class != PersistObjectClass: throw "TypeError"
                    return object
                object_new(name)
            )
        }
        List: {
            new: (from ->
                list = list_new()
                if from: from.each: e -> object.add(e)
                return list
            )
            get: (name ->
                list = ids[name]
                if list:
                    if list.class != PersistListClass: throw "TypeError"
                    return list
                list_new(name)
            )
        }
        /*
        Var: {
            new: -> var_new()
            get: (name ->
                var = ids[name]
                if var:
                    if var.class != PersistVarClass: throw "TypeError"
                    return var
                var_new(name)
            )
        }
        */
    }
}

return { new: new }

