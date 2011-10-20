if = (c, block){ bool(c, block, {})() }
loop = (n){
    if n <= 0: return
    if n <= 0: error
    print("N:", n)
    goto loop n - 1
}
loop 100

