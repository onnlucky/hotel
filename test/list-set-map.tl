x = %list_new(1, 2, 3)
out(%list_get(x, 2), " == 3\n")

y = %map_new(#foo, 1, #bar, 2, #baz, 3)
out(%map_get(y, #baz), " == 3\n")
out(%map_get(y, #bar), " == 2\n")
out(%map_get(y, #foo), " == 1\n")
out(%map_get(y, #not), " == undefined\n")

z = %set_new(#foo, 1, 2, 3, #baz)
out(%set_has(z, #foo))
out(%set_has(z, 1))
out(%set_has(z, 4), "\n")

test = %list_new(
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "x", "y", "z"
)

out(%list_size(test), "\n")
out(%list_get(test, %list_size(test) - 1), "\n")
out(%list_get(test, 9), "\n")
out(%list_get(test, 19), "\n")
