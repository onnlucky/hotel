if = (cond, &tb, &fb){ bool(cond, tb, fb)() }

manymany = (n){
    l = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
    if(n % 333 == 0, out(n, " ", l.size, "\n"))
    if(n >= 10000, return())
    goto(manymany(n + 1))
}
manymany(0)
