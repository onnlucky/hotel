res = HashMap.new
test = n ->
    res[n] = "load"
    catch: e ->
        res[n] = res[n] +" fail: "+ e.toString
    code = io.File(n).readString
    res[n] = "parse"
    parse = parse(code)
    print repr parse
    res[n] = "compile"
    bytes = compiler.compile(code)
    res[n] = "load"
    mod = compiler.load(bytes)
    res[n] = "ok"
    mod.run

test("bad.tl")

/*
tests = "test/noboot/"
io.Path(tests).each: n ->
    if not n.endsWith(".tl"): continue
    test(tests + n)

tests = "test/"
io.Path(tests).each: n ->
    if not n.endsWith(".tl"): continue
    test(tests + n)

tests = "modules/"
io.Path(tests).each: n ->
    if not n.endsWith(".tl"): continue
    test(tests + n)

tests = "./"
io.Path(tests).each: n ->
    if not n.endsWith(".tl"): continue
    test(tests + n)

test("boot/boot.tl")

res.keys.sort.each: k -> print k, res[k]

/*
bytes = compiler.compile(code)
mod = compiler.load(bytes)
print "loaded ...", bytes.size, bytes
env = Env.current

ls = mod.links.map: g ->
    x = env.get(g)
    if x: return x
    x = resolve(g)
    if x: return x
    null

mod.link(ls)
mod.run

