if = c, block -> bool(c, block, (->))()
loop = n ->
    if n <= 0: return
    if n <= 0: error
    out("N: ", n, "\n")
    goto loop n - 1
loop 100

