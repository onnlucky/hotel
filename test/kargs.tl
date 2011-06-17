print = (a1, *rest, **options){
    out(a1, " ")
    out(%list_size(rest), " ")
    out(%map_size(options), " ")
    out(%map_get(options, #newline), " ")
    out(%map_get(options, #foobar), " ")
    out("\n")
}

print("hello", 1, 2, 3, newline: true, foobar: false, something: 199 + 188)
print(newline: false)
print()
