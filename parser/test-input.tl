if = { c, &t, &f -> bool(c, t, f)() }

fib = { n ->
    if(n < 2, 1, fib(n - 1) + fib(n - 2))
}
a = fib(1)
print a

