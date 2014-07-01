_module_run(_Module_new(_bufferFromFile("boot/fase1.tlb"), "fase1"))
print "DID FASE 1"
_module_run(_Module_new(_bufferFromFile("boot/fase2.tlb"), "fase2"))
print "DID FASE 2"
_module_run(_Module_new(_bufferFromFile("boot/fase3.tlb"), "fase3"))


/*
env = Env.current

code = io.File("compiler.tl").readString
bytes = compiler.compile(code)
mod = compiler.load(bytes)
links = mod.links.map: link ->
    ref = env.get(link) or Env.module(link)
    print "link:", link, ref
    assert ref
    ref
mod.link(links)

print "LOADING COMPILER"
compiler2 = mod.run
print "COMPILING CODE AGAIN"
bytes2 = compiler2.compile(code)
print "LOADING CODE AGAIN"
mod2 = compiler2.load(bytes)
print "LINKING CODE AGAIN"
links = mod.links.map: link ->
    ref = env.get(link) or Env.module(link)
    print "link:", link, ref
    assert ref
    ref
mod.link(links)

print "LOADING COMPILER AGAIN"
compiler2 = mod.run
print "COMPILING CODE AGAIN AGAIN"
bytes2 = compiler2.compile(code)
print "LOADING CODE AGAIN AGAIN"
mod2 = compiler2.load(bytes)
print "LINKING CODE AGAIN AGAIN"
links = mod.links.map: link ->
    ref = env.get(link) or Env.module(link)
    print "link:", link, ref
    assert ref
    ref
mod.link(links)

