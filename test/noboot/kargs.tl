print = a1 ->
    out(a1, " ")
    out(args.size, " ")
    out(args.namesmap.size, " ")
    out(args.namesmap.get("newline"), " ")
    out(args.namesmap.get("foobar"), " ")
    out("\n")

print("hello", 1, 2, 3, newline=true, foobar=false, something=199 + 188)
print(newline=false)
print()

