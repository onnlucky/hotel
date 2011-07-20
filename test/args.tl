testargs = (first, second)->{
    out(first, second, _args_size(args) - 2, "\n")
}
testargs("three", " size: ", #foo, #bar, #baz)
testargs("zero", " size: ")
