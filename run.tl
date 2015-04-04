in = "
Shape = { ->
    print 1
    print = -> print \"!print!\", this.x, this.y
}
Test = { ->
    x = 42
    y = 100
    print 0
    extend Shape()
    print 2
}
o = Test()
print 3
o.print
print 4
o.xxx
print 5
"
m = testcompiler.load(in, _env_current())
m.disasm
m.run
