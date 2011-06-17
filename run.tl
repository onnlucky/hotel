if = (cond, &tb, &fb){ bool(cond, tb, fb)() }
fib = (n){
    if n <= 2, return(1)
    (fib n - 1) + (fib n - 2)
}
print fib 3

print [4, 5, 6]
//print [4, 5, 6][0]
//print [4, 5, 6].first
