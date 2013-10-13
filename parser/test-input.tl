if = { c, &t, &f -> bool(c, t, f)() }
fib = { n ->
    if(n <= 2, 1, fib(n - 1) + fib(n - 2))
}
print fib(30)

