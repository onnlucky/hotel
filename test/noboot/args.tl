testargs = first, second -> out(first, second, args.size - 2, "\n")
testargs("three", " size: ", "foo", "bar", "baz")
testargs("zero", " size: ")


test1 = a1, a2, a3 -> out a1, " ", a2, " ", a3, "\n"
test2 = -> out args[1], " ", args[2], " ", args[3], "\n"
test3 = -> out args[2:].size, "\n"
test4 = -> out args["foo"], "\n"

test1(42, 43, foo=44)
test2(42, 43, foo=44)
test3(42, 43, foo=44)
test4(42, 43, foo=44)

