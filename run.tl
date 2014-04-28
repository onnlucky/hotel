code = io.File("run.tl").readString
bytes = compiler.compile(code)
mod = compiler.load(bytes)
print "loaded ...", bytes.size, bytes
env = Env.current

mod.links.each: g ->
    x = env.get(g)
    if x:
        print g, "->", x
        return
    x = resolve(g)
    print g, "->", x
//mod.run

