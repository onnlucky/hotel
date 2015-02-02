new = name ->
    fd = io.File(name).open(create=true)
    ids = HashMap.new

    record = cmd -> fd.write(_to_repr(cmd), "\n")

    PersistObjectClass = {
        _name = "PersistObjectClass"

        _set = k, v -> this._back.set(k, v); record {set=true,id=this._id,k=k,v=v}; return v
        _get = -> this._back.get(args.msg)
    }

    PersistListClass = {
        _name = "PersistListClass"

        add = v -> _, i = this._back.add(v); record {set=true,id=this._id,k=i,v=v}; return i
        set = i, v -> this._back.set(i, v); record {set=true,id=this._id,k=i,v=v}; return v
        remove = at -> r = this._back.remove(at); record {del=true,id=this._id,k=at}; return r

        // notice these are "delegates" to the _black-ing store
        size= -> this._back.size
        get = i -> this._back.get(i)
        each= fn -> this._back.each(args.block or fn)
        find= fn -> this._back.find(args.block or fn)
        map= fn -> this._back.map(args.block or fn)
    }

    genid = ->
        loop:
            id = random()
            if not ids.has(id): return id

    object_new = id, silent ->
        id = id or genid()
        if not silent: record {new="Object", id=id}
        res = {_id=id, _back=HashMap.new, class=PersistObjectClass}
        ids.set(id, res)

    list_new = id, silent ->
        id = id or genid()
        if not silent: record {new="List", id=id}
        res = {_id=id, _back=Array.new, class=PersistListClass}
        ids.set(id, res)

    revive = v ->
        if isObject(v):
            if try(v.class._name) == "PersistObjectClass": return ids[v._id]
            if try(v.class._name) == "PersistListClass": return ids[v._id]
        return v

    replay = cmd ->
        { cmd?new == "Object" }: object_new(cmd.id, true)
        { cmd?new == "List"   }: list_new(cmd.id, true)
        { cmd?set             }: ids[cmd.id]._back.set(cmd.k, revive(cmd.v))
        { cmd?del             }: ids[cmd.id]._back.remove(cmd.k)
        {                     }: Error("unknown command: $(repr cmd)").throw

    while fd.find("\n"):
        cmd = _from_repr(fd.rbuf)
        assert fd.read(1) == "\n"
        replay(cmd)

    return {
        Object = {
            new = from ->
                object = object_new()
                if from: Map(from).each: k, v -> object._set(k, v)
                return object
            get = name ->
                object = ids[name]
                if object:
                    if object.class != PersistObjectClass: Error("item with '$name' is not an object").throw
                    return object
                object_new(name)
        }

        List = {
            new= from ->
                list = list_new()
                if from: from.each: e -> list.add(e)
                return list
            get= name ->
                list = ids[name]
                if list:
                    if list.class != PersistListClass: Error("item with '$name' is not a list")
                    return list
                list_new(name)
        }
        /*
        Var= {
            new= -> var_new()
            get= (name ->
                var = ids[name]
                if var:
                    if var.class != PersistVarClass: throw "TypeError"
                    return var
                var_new(name)
            )
        }
        */
    }
{{ new }}

