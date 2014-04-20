x = [1, 2, 3]
out(x[3], " == 3\n")

y = [foo = 1, bar = 2, baz = 3]
out(y[#baz], " == 3\n")
out(y[#bar], " == 2\n")
out(y[#foo], " == 1\n")
out(y[#not], " == undefined\n")

//z = %set_new(#foo, 1, 2, 3, #baz)
//out(%set_has(z, #foo))
//out(%set_has(z, 1))
//out(%set_has(z, 4), "\n")

test = [
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "x", "y", "z"
]

out(test.size, "\n")
out(test[test.size], "\n")
out(test.get(10), "\n")
out(test.get(20), "\n")
