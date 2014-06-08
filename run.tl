res = HashMap.new
test = n ->
    output = Array.new
    out = -> args.each(a -> output.add(a)); print repr ["out>"] + args[:]
    catch: e ->
        res[n] = res[n] +" fail: "+ e.toString
        print "---- $n: FAIL :( ----"
        print e.toString.strip
        print "---- $n: FAIL :( ----"
        throw "STOP"
    print "---- $n ----"
    res[n] = "load"
    code = io.File(n).readString
    print code.strip
    print "---- $n ----"
    res[n] = "parse"
    parse = parse(code)
    print repr parse
    res[n] = "compile"
    bytes = compiler.compile(code)
    res[n] = "load"
    mod = compiler.load(bytes, out)
    res[n] = "run"
    r = mod.run
    res[n] = "compare"
    print "---- $n: output ----:"
    print repr output.toList
    ref = io.File(n[:-3] + "ref").readString
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

/*
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

