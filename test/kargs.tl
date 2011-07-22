print = (a1)->{
    out(a1, " ")
    out(_args_size(args), " ")
    out(_map_size(_args_names(args)), " ")
    out(_map_get(_args_names(args), #newline), " ")
    out(_map_get(_args_names(args), #foobar), " ")
    out("\n")
}

print("hello", 1, 2, 3, newline=true, foobar=false, something=199 + 188)
print(newline=false)
print()
