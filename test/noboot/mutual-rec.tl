if = cond, &tb, &fb -> bool(cond, tb, fb)()
even = n ->
    if(n <= 0, return(true), goto(odd(n - 1)))
odd = n ->
    if(n <= 0, return(false), goto(even(n - 1)))
print even(10), odd(10)
