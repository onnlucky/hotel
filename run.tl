if = ( cond, &tb, &fb => bool(cond, tb, fb)() )
fib = ( n =>
    if(n <= 2, return(1))
    fib(n - 1) + fib(n - 2)
)
fib(20)
