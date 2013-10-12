/*
one = { -> two() }
two = { -> print("yes") }
three = { -> print("FORGET_ME_NOT!!") }
one()
*/

if = { c, &t, &f -> bool(c, t, f)() }
fib = { n ->
    if(n < 2, 1, fib(n - 1) + fib(n - 2))
}
print "ANSWER:", 1, fib(1)
print "ANSWER:", 2, fib(2)
print "ANSWER:", 3, fib(3)
print "ANSWER:", 4, fib(4)
print "ANSWER:", 9, fib(9)

// 0  1  2  3  4  5   6   7   8   9
// 1, 1, 2, 3, 5, 8, 13, 21, 34, 55
