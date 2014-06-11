x = 100
test1 = -> out(x, "\n")
x2 = 200
test2 = -> out(x2, "\n")
x3 = 300
test3 = -> out(x3, "\n")
x4 = 400
test4 = -> out(x4, "\n")

test1()
test2()
test3()
test4()

x = test1

test3()
test2()
x()
