if = ( cond, &tb, &fb => bool(cond, tb, fb)() )
fib = ( n =>
    if(lte(n, 2), return(1))
    add(fib(sub(n, 1)), fib(sub(n, 2)))
)
fib(20)

