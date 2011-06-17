testargs = (first, second, *rest){
    out(first, second, %list_size(rest), "\n")
}
testargs("three", " size: ", #foo, #bar, #baz)
testargs("zero", " size: ")
