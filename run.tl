code = io.File("run.tl").readString
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

