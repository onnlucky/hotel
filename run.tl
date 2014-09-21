_env = Env.current

res = HashMap.new
test = n ->
    output = Array.new
    out = -> args.each(a -> output.add(str(a)))
    catch: e ->
        res[n] = res[n] +" fail: "+ e.toString
        print "---- $n: FAIL :( ----"
        print e.toString.trim
        print "---- $n: FAIL :( ----"
        //throw "STOP"
    res[n] = "load"
    code = io.File(n).readString
    res[n] = "compile"
    bytes = compiler.compile(code)
    res[n] = "load"
    mod = compiler.load(bytes, n)
    links = mod.links.map(link ->
        if link == "out": return out
        _env.get(link) or try(Env.module(link), compiler.unknown)
    )
    nmod = mod.link(links)
    res[n] = "run"
    r = nmod.run
    if r: out(r, "\n")
    res[n] = "compare"
    print "---- $n: output ----:"
    print output.map(e -> str(e)).join("")
    ref = try(io.File(n[:-3] + "ref").readString)
    if ref:
        print "---- $n: ref ----:"
        print ref
        assert ref == output.join("")
    res[n] = "ok"
    print "---- $n: OK! ----"
    print()

test("bad.tl")

tests = "test/noboot/"
io.Path(tests).each: n ->
    if not n.endsWith(".tl"): continue
    test(tests + n)


tests = "test/"
io.Path(tests).each: n ->
    if n == "tester.tl": continue
    if not n.endsWith(".tl"): continue
    test(tests + n)

/*
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

