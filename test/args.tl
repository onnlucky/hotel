testargs = (first, second){
    out(first, second, args.size - 2, "\n")
}
testargs("three", " size: ", #foo, #bar, #baz)
testargs("zero", " size: ")
