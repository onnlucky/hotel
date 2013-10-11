//if = { c, &t, &f -> bool(c, t, f)() }
//if("cond",print("yes"),print("no"))

test = print
test("1 2 3")
{ -> test("hello") }()

