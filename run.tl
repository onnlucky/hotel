# boot.tl
# 1. initialize into full std lib (some of it is in controlflow.c)
# 2. run the requests file(s) or start a repl
# 3. drop into the io runloop, exiting when no tasks are left

catch: e -> print "boot exception: $(e.toString)"

// TODO use args["block", else=t]
if = cond, &t, &f -> bool(cond, args.get("block", t), args.get("else", f))()

_or = left, &right ->
    if not left: return right()
    return left
_install_global("or", _or)

_and = left, &right ->
    if left: return right()
    return left
_install_global("and", _and)

while = &cond ->
    block = args.block; if not block: throw "while expects a block"
    loop:
        if not cond(): break
        block()

// TODO otherwise syntax is useful in object?(else=42)test
try = &test, &otherwise ->
    catch: return otherwise()
    if args.block: return args.block.call
    test()

asBool = v -> bool(v, true, false)
asBoolInv = v -> bool(v, false, true)
isSmaller = l, r -> return compare(l, r) == -1

min = l, r ->
    if l <= r: return l
    return r

max = l, r ->
    if l >= r: return l
    return r

print "boot 1"
_test = if true: "boot"
print "test"
assert _test

_test = ->
    assert args[1] == 42
    assert args["name"] == "hello"
    assert args[:][1] == 42
    assert args.names.name == "hello"
_test(name="hello", 42)
print "boot 2"
_test(42, name="hello")
print "boot 3"
_test(name="hello", 42)
print "boot 4"
_test(foo="bar", 42, name="hello")
print "boot 5"

/// List.random(): return a random element from the list
_get_random = -> this.get(1 + random(this.size))

/// List.each([fn]): apply a function to all elements, returns nothing
/// [block] the block to call, it is passed in the element and the position as `block(e, at)`
/// if no block is given, #fn will be called, but without the position, `fn(e)`
/// > [1,2,3].each(e -> print(e)) == null // will print 1, 2 and 3 each on their own line
_each = fn ->
    block = args.block
    if not (block or fn): throw "each expects a block or fn"

    size = this.size
    at = Var.new 0
    while $at < size:
        $at += 1
        if block: block(this[$at], $at)
        if not block: fn(this[$at])

/// List.map([fn]): apply a function to all elements, return a list of the results
/// [block] the block to call, it is passed in the element and position as `block(e, at)`
/// if no block is given, #fn will be called, but without the position, `fn(e)`
/// > [1,2,3].map(e -> e * 3) == [3,6,9]
_map = fn ->
    block = args.block
    if not (block or fn): throw "map expects a block or fn"

    size = this.size
    if size == 0: return []

    res = _List_unsafe(size)
    loop = at =>
        if at > size: return res
        if block: _list_set_(res, at, block(this[at], at))
        if not block: _list_set_(res, at, fn(this[at]))
        goto(loop(at + 1))
    loop(1)

/// List.find(element): find #element, return its position or #null if not found
/// [from] start searching from here instead
/// [block] if not given an element but a block, returns first element for which `block(e, at)` is true
/// > [1,2,3,4].find(4) == 4
/// > [1,2,3,4].find(e -> e > 2) == 3
_find = element /*, [Number from=0]*/ ->
    size = this.size
    if size == 0: return
    at = Var.new args["from"] or 1
    if $at < 0: $at = size + 1 + $at
    if $at < 1: $at = 1
    if args.size == 0:
        // passed in a block
        block = args.block
        if not block: throw "find expects an element or a block"
        while $at <= this.size:
            if block(this[$at], $at): return $at
            $at += 1
        return
    // passed in an element
    while $at <= this.size:
        if this[$at] == element: return $at
        $at += 1
    return

/// List.filter(elements*): remove all elements given
/// [block] if given a block, also remove all elements for which `block(e, at)` is true
/// if called without arguments and without a block, it filters out all falsy values
/// > [1,2,3,4].filter(3,1) == [2,4]
/// > [1,2,3,4].filter(e -> e % 3 == 0) == [1,2,4]
/// > ["hello",null].filter == ["hello"]
_filter = ->
    len = this.size
    if len == 0: return []
    block = { args.block     }: args.block
            { args.size == 0 }: asBoolInv
            {                }: null
    arr = Array.new
    loop = at =>
        if at > len: return arr.toList
        e = this[at]
        if not args.find(e) and (not block or not block(e, at)): arr.add(e)
        goto(loop(at + 1))
    loop(1)

/// List.reduce([fn]): reduce a list to a single value using a function
/// [block] either using the passed in block or #fn
/// [initial] the initial value, will be the first value if not given
/// the function is called with two arguments, an intermediate result, and the next element
/// the result of the call is the next intermediate result
/// reducing an empty list returns null (nothing)
/// reducing a list with a single element returns that element
/// > [1,2,3].reduce(tmp, next -> tmp + next) == 6
// TODO allow passing in the initial value
_reduce = fn ->
    block = args.block or fn
    if not block: throw "reduce expects a block or fn"

    var $i = 1
    var $res = null
    { args.has("initial") }: $res = args["initial"]
    {                     }: $res = this[1]; $i = 2

    while $i <= this.size:
        $res = block($res, this.get($i))
        $i += 1
    return $res

/// List.sum: return the addition of all elements using '+'
_sum = ->
    var $sum = 0
    this.each: e -> $sum += e
    return $sum

/// List.min: return the smallest element
_min = default ->
    if this.size == 0: return args[1]
    if this.size == 1: return this[1]
    var $min = this[1]
    this.each: e -> if compare(e, $min) == -1: $min = e
    return $min

/// List.max: return the largest element
_max = default ->
    if this.size == 0: return args[1]
    if this.size == 1: return this[1]
    var $max = this[1]
    this.each: e -> if compare(e, $max) == 1: $max = e
    return $max

/// List.mean: return the sum divided by size (the average)
_mean = default ->
    if this.size == 0: return args[1]
    this.sum / this.size

/// List.median: return the middle element
_median = default ->
    if this.size == 0: return args[1]
    if this.size == 1: return this[1]
    sorted = this.sort
    half = this.size / 2
    if half.floor == half: return sorted[half]
    (sorted[half.floor] + sorted[half.ceil]) / 2

innerqsort = list, smallerfn, b, e ->
    if e - b < 1: return
    pivot = list[b]
    at = Var.new b + 1
    end = Var.new e
    while $at <= $end:
        check = list[$at]
        { smallerfn(check, pivot) }: list[$at - 1] = check; $at += 1
        {                         }: list[$at] = list[$end]; list[$end] = check; $end -= 1
    list[$end] = pivot
    innerqsort(list, smallerfn, b, $end - 1)
    innerqsort(list, smallerfn, $end + 1, e)

qsort = list, smallerfn ->
    if isArray(list):
        innerqsort(list, smallerfn, 1, list.size)
        return list
    list = Array.new(list)
    innerqsort(list, smallerfn, 1, list.size)
    return list.toList

/// List.sort: return a sorted list
/// [block] use passed in block as comparison function, otherwise sort small to large
_sort = fn -> qsort(this, args.block or fn or isSmaller)

/// Number: given a string, it parses it to a number or returns null
/// given a number it returns the number
/// returns null otherwise
Number = s ->
    if isNumber(s): return s
    // TODO regex for [0-9_-+]
    v = try(_json_parse(s.toString))
    if isNumber(v): return v
    return null

// TODO only flatten one level, unless asked?
_flatten2 = ls, arr ->
    ls.each(e ->
        if isList(e): _flatten2(e, arr); return
        if isArray(e): _flatten2(e, arr); return
        arr.add(e)
    )

/// List.flatten: return a single list where all elements that are lists themselves have been merged
/// at the moment, flatten will merge all such lists before merging their elements
/// > [[1,2],3,[4,5]].flatten == [1,2,3,4,5]
_flatten = ->
    arr = Array.new
    _flatten2(this, arr)
    return arr.toList

/// List.join(sep=""): return a #String separated by #sep
/// > ["hello", "world"].join(" ") == "hello world"
_join = sep ->
    if this.size == 0: return ""
    sep = sep or ""

    var $res = this[1].toString
    var $i = 2
    while $i <= this.size:
        $res = $res.cat(sep).cat(this[$i].toString)
        $i += 1
    return $res

_addAll = ls -> ls.each(e -> this.add(e))

/// String.split(on): return a #List with the #String broken up at every occurance of #on
/// [keepempty] option to keep empty results
/// [trim] trim the results
/// [limit] limit how many times to break up the string
/// > "hello world".split == ["hello","world"]
/// > "a, b, , d, e".split(",",trim=true) == ["a", "b", "d", "e"]
// TODO add from, add backward
// TODO optimize both find and process, the conditions are static
_split = on ->
    // TODO on should support a regex and default to breaking whitespace
    on = on or " " // TODO Regex("\\s+")
    empty = bool(args.has("keepempty"),  args["keepempty"], true)
    trim = args["trim"]
    limit = args["limit"]

    res = Array.new

    find = start ->
        { isRegex(on) }: b, e = on.find(this, start)
                         if not b: return
                         return b, e - b + 1
        {             }: return this.find(on, start), on.size

    process = s ->
        s = { isString(trim) or isRegex(trim) }: s.trim(trim)
            { trim                            }: s.trim
            {                                 }: s
        if not empty and s.size == 0: return
        res.add(s)

    loop = start ->
        end, len = find(start)
        if not end:
            process(this[start:])
            return
        if end >= start:
            process(this[start:end - 1])
            if limit and res.size >= limit - 1:
                process(this[end + len:])
                return
            goto loop(end + len)
    loop(1)
    return res.toList

/// Number.times([fn]): call a block or #fn so many times
/// > 10.times: print "hello" /// will print hello ten times
_times = fn ->
    if this <= 0: return
    block = args.block or fn
    if not block: throw "times expects a block or fn"

    at = Var.new 0
    loop:
        if $at >= this: return
        $at += 1
        block($at)

/// Number.to(to, [fn]): call a block or #fn by counting upto #to
/// > 10.to(15): print "hello" /// wil print hello 5 times
_to = to, fn ->
    if not isNumber(to): throw "to expects a number"
    block = args.block or fn
    if not block: throw "to expects a block or fn"

    from, to = { this <= to }: multi(this - 1, to)
               {            }: multi(to - 1, this)

    at = Var.new from
    loop:
        if $at >= to: return
        $at += 1
        block($at)

_install(Int._methods, "times", _times)
_install(Int._methods, "to", _to)

_str = o ->
    if isString(o): return o
    if isBin(o): return o.toString
    if isBuffer(o): return o.readString
    if isChar(o): return o.toString
    // TODO these are weird ...
    if isNumber(o): return o.toChar
    if isList(o) or isArray(o): return o.flatten(true).toChar
    return "$o"

/// String():
/// call with numbers, interprets them as letters (unicode points)
/// call with binaries or buffers, interprets them as utf8 letters
/// TODO allow configuration on what to do for invalid utf8 and/or invalid unicode points
_install(String, "call", (new = (->
    if args.size == 0: return ""
    if args.size == 1: return _str(args[1])
    arr = Array.new
    args.each: a -> arr.add(_str(a))
    arr.join
)))

// TODO should have special boot places for string, list and such
/// String.each: for ever character run block or function given
_install(String._methods, "times", (times = (n ->
    arr = Array.new
    n.times: arr.add(this)
    arr.join("")
)))
_install(String._methods, "each", _each)
_install(String._methods, "map", _map)
_install(String._methods, "random", _get_random)

_install(Bin._methods, "find", _find)
_install(Bin._methods, "each", _each)
_install(Bin._methods, "map", _map)
_install(Bin._methods, "random", _get_random)

/// String.split: split the string wherever args[1] occurs, returns a list of string fragments
_install(String._methods, "split", _split)
/// String.replace: replace all occurances of args[1] with args[2], returns a new "String"
_install(String._methods, "replace", (replace = (from, to ->
    if from.size == 0: return this
    if not this.find(from): return this
    return this.split(from).join(to)
)))

_install(Args._methods, "each", _each)
_install(Args._methods, "map", _map)
_install(Args._methods, "find", _find)
_install(Args._methods, "reduce", _reduce)
_install(Args._methods, "filter", _filter)
_install(Args._methods, "flatten", _flatten)
_install(Args._methods, "join", _join)

_install(Set._methods, "random", _get_random)
_install(Set._methods, "each", _each)
_install(Set._methods, "map", _map)
_install(List._methods, "random", _get_random)
_install(List._methods, "each", _each)
_install(List._methods, "map", _map)
_install(List._methods, "find", _find)
_install(List._methods, "filter", _filter)
_install(List._methods, "reduce", _reduce)
_install(List._methods, "min", _min)
_install(List._methods, "max", _max)
_install(List._methods, "sum", _sum)
_install(List._methods, "mean", _mean)
_install(List._methods, "median", _median)
_install(List._methods, "sort", _sort)
_install(List._methods, "flatten", _flatten)
_install(List._methods, "join", _join)

_install(Map._methods, "each", (each = (fn ->
    block = args.block or fn
    if not block: throw "each expects a block or fn"
    this.keys.each: k, n -> block(k, this[k], n)
)))
_install(Map._methods, "map", (map = (fn ->
    block = args.block or fn
    if not block: throw "map expects a block or fn"
    this.keys.map: k, n -> block(k, this[k], n)
)))
_object_each = (m, fn ->
    block = args.block or fn
    if not block: throw "each expects a block or fn"
    keys = Object.keys(m)
    keys.each: k, n -> block(k, Object.get(m, k), n)
)
_object_map = (m, fn ->
    block = args.block or fn
    if not block: throw "map expects a block or fn"
    keys = Object.keys(m)
    keys.map: k, n -> block(k, Object.get(m, k), n)
)
_install(Object, "each", _object_each)
_install(Object, "map", _object_map)

_hashmap_map = (fn ->
    block = args.block or fn
    if not block: throw "map expects a block or fn"
    res = Array.new
    this.each(k, v, n -> res.add(block(k, v, n)))
    res.toList
)
_install(HashMap._methods, "map", _hashmap_map)

_install(Array._methods, "addAll", _addAll)
_install(Array._methods, "random", _get_random)
_install(Array._methods, "each", _each)
_install(Array._methods, "map", _map)
_install(Array._methods, "find", _find)
_install(Array._methods, "filter", _filter)
_install(Array._methods, "reduce", _reduce)
_install(Array._methods, "min", _min)
_install(Array._methods, "max", _max)
_install(Array._methods, "sum", _sum)
_install(Array._methods, "mean", _mean)
_install(Array._methods, "median", _median)
_install(Array._methods, "sort", _sort)
_install(Array._methods, "flatten", _flatten)
_install(Array._methods, "join", _join)

_errorConstructor = msg ->
    stack = Task.stacktrace(1)
    args.namesmap.set("stack", stack).set("msg", msg).set("class", this.class).toObject

_install(Error, "call", _errorConstructor)
_install(ArgumentError, "call", _errorConstructor)
_install(UndefinedError, "call", _errorConstructor)
_install(Error, "class", {
    throw = -> throw this
    toString = ->
        ls = Array.new
        ls.add "Error: ", this.msg, "\n"
        ls.add("stack: ", this.stack.task, "\n")
        x = Var.new 1
        loop:
            file, fn, line = this.stack.get($x)
            if not file: break
            ls.add "  at ", file, ":", line, " ", fn, "\n"
            $x += 1
        return ls.join
})
_install(DeadlockError, "class", {
    throw = -> throw this
    toString = ->
        ls = Array.new
        ls.add "Error: ", this.msg, "\n"
        this.stacks.each: stack, n ->
            ls.add("stack: ", n, " ", stack.task, "\n")
            x = Var.new 1
            loop:
                file, fn, line = stack.get($x)
                if not file: break
                ls.add "  at ", file, ":", line, " ", fn, "\n"
                $x += 1
        return ls.join
})

assert [1,[2,[]],"hello",[]].flatten == [1,2,"hello"]

dirname = file ->
    at = file.find("/", backward=true)
    if not at: return "."
    if at == 1: return "/"
    slash = Var.new(at - 1)
    while file[$slash] == '/': $slash -= 1
    return file[:$slash]

assert dirname("hello.html") == "."
assert dirname("/hello.html") == "/"
assert dirname("./hello.html") == "."
assert dirname("../hello.html") == ".."
assert dirname("../foo/test/hello.html") == "../foo/test"
assert dirname("//foo/..//../test///hello.html") == "//foo/..//../test"

filename = file ->
    at = file.find("/", backward=true)
    if not at: return file
    return file[at + 1:]

assert filename("hello.html") == "hello.html"
assert filename("/hello.html") == "hello.html"
assert filename("./hello.html") == "hello.html"
assert filename("../foo/test/hello.html") == "hello.html"

sleep = s -> io.wait s
wait  = s -> io.wait s

// TODO Env.current
_env = _env_current()

sp = "hello world".split
assert sp.size == 2, sp[1] == "hello", sp[2] == "world"

// TODO should be immutable
_module_paths = Array.new

// default module path
(_io_getenv("TL_MODULE_PATH") or "/usr/local/lib/tl").split(":").each(p -> _module_paths.add(p))

// TODO should be keyed on start env?
_modules = HashMap.new

// resolve the first file from a certain module path
fileFromModulePath = name, path ->
    if path:
        file = "$path/$name"
        if _Path_stat(file).ino != 0: return file
    _module_paths.each: p =>
        file = "$p/$name"
        if _Path_stat(file).ino != 0: return file
    return null

assert [3000, 19, 10, 99, "b", "c", "a"].sort[1] == 10
(
    catch: e -> print "catching ok"
    null - 1
    assert false
)
print "OK!"
//return

compiler = (
    file = fileFromModulePath("compiler.tlb")
    if not file: Error("No compiler.tlb found").throw
    bcode = _bufferFromFile(file)
    mod = _Module_new(bcode, file[:-2])
    links = _module_links(mod)
    linked = links.map(name -> _env[name])
    //linked.each: value, n -> print n, links[n], value
    compiler = _module_run(_module_link(mod, linked))

    print("reloading compiler")
    text = _stringFromFile("modules/compiler.tl")
    start = time()
    ast = compiler.parse(text)
    print("parsed:", time() - start)
    bcode = compiler.compile(text, ast)
    print("compiled:", time() - start)
    compiler = _module_run(_module_link(_Module_new(Buffer.new(bcode), file[:-2]), linked))
    print("linked and run", time() - start)
    compiler
)

resolve2 = name, path ->
    res = _modules[name]
    if isDefined res: return res
    print("RESOLVING 2: $name ($path)")
    catch: e => return null, "Unable to read module '$name': $(e.msg)"
    file = fileFromModulePath("$name.tlb", path)
    if file:
        print("binary module", file)
        catch: e => return null, "Unable to load module '$name': $(e.toString)"
        path = dirname(file)
        mod = compiler.load(_bufferFromFile(file), name=file[:-2], _env, (name -> resolve(name, path)))
        catch: e => return null, "Unable to run module '$name': $(e.msg)"
        res = mod.run()
        return _modules.set(name, res)
    file = fileFromModulePath("$name.tl", path)
    if file:
        print("module", file)
        catch: e => return null, "Unable to compile module '$name': $(e.toString)"
        path = dirname(file)
        mod = compiler.load(_stringFromFile(file), name=file, _env, (name -> resolve(name, path)))
        catch: e => return null, "Unable to run module '$name': $(e.msg)"
        res = mod.run()
        return _modules.set(name, res)
    file = fileFromModulePath("$name.mod", path)
    if file:
        print("native module", file)
        catch: e => return null, "Unable to load compiled module '$name': $(e.msg)"
        res = dlopen(file)
        return _modules.set(name, res)
    return null, "Unable to read module '$name': no module found"

resolve = name, path ->
    res, fail = resolve2(name, path)
    if res: return res
    if fail: throw Error(fail)

compile = resolve("compiler")
io = resolve("io")
_env["io"] = io

_eval = env/*env*/ ->
    { env }: compiler.load(this, name="<eval>", env).run()
    {     }: compiler.load(this, name="<eval>", _env, (name -> resolve(name, null))).run()

/// String.eval(Env env): evalute the string as a script to run in the passed in scope, or a default scope
_install(String._methods, "eval", _eval)

print "HELLO WORLD!"
//_env.each: k, v -> print k, v

"x = 42\nprint x".eval

/*

// run a repl, or every argument as a file
runmain = as ->
    catch: e -> print e.toString; _set_exitcode(1)

    if as.size == 0: resolve("commandline")()
    //if as[1] == "snippets": return snippets.run

    file = as[1]
    path = dirname(file)
    return compiler.load(_stringFromFile(file), name=file, _env, (name -> resolve(name, path))).run(as[2:])

main = !runmain([])

// this will only return when there are no more tasks to run
io.ioloop

// we are done, print the results, if any, error has already been printed
if main.value: print main.value
return main.value

