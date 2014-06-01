res = HashMap.new
test = n ->
    res[n] = "load"
    catch: e ->
        res[n] = res[n] +" fail: "+ e.toString
    print "---- $n ----:"
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
/**/
res.keys.sort.each: k -> print k, res[k]

