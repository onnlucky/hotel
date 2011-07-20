out(out("hello ","world\n"))
out("\n")

foo = (t)->{ 42 }
bar = (&t)->{ t() }
out(bar(foo(20)))
out("\n")

foo1 = (t)->{ 42 }
bar1 = (&t)->{ out(t()) }
bar1(foo1(10))
out("\n")
