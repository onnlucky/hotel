testargs = (first, second)->{
    out(first, second, _args_size(args), "\n")
}
testargs("three", " size: ", #foo, #bar, #baz)
testargs("zero", " size: ")
