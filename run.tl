in = "
Test = { ->
    x = 10
    print = -> print this.x
    extend String(\"hello\")

    class TEST = 100
}
"
testcompiler.load(in, _env_current()).disasm
