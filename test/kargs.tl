print = ( a1 ->
    out(a1, " ")
    out(args.size, " ")
    out(args.map.size, " ")
    out(args.map.get(#newline), " ")
    out(args.map.get(#foobar), " ")
    out("\n")
)

print("hello", 1, 2, 3, newline=true, foobar=false, something=199 + 188)
print(newline=false)
print()

