testargs = first, second -> out(first, second, args.size - 2, "\n")
testargs("three", " size: ", #foo, #bar, #baz)
testargs("zero", " size: ")

test = -> args.block.call(42, 43, foo=44)

test: a1, a2, a3 -> print a1, a2, a3
test: -> print args[1], args[2], args[3]
test: -> print args[2:].size
test: -> print args[#foo]

