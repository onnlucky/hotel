in = "
Test = { ->
    x = 42
    print = -> print this.x
}

o = Test()
o.print
o.print
"
m = testcompiler.load(in, _env_current())
m.disasm
m.run
