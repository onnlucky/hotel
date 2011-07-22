x = [1, 2, 3]
out(_list_get(x, 2), " == 3\n")

y = { foo = 1, bar = 2, baz = 3 }
out(_map_get(y, #baz), " == 3\n")
out(_map_get(y, #bar), " == 2\n")
out(_map_get(y, #foo), " == 1\n")
out(_map_get(y, #not), " == undefined\n")

//z = %set_new(#foo, 1, 2, 3, #baz)
//out(%set_has(z, #foo))
//out(%set_has(z, 1))
//out(%set_has(z, 4), "\n")

test = [
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "x", "y", "z"
]

out(_list_size(test), "\n")
out(_list_get(test, _list_size(test) - 1), "\n")
out(_list_get(test, 9), "\n")
out(_list_get(test, 19), "\n")
