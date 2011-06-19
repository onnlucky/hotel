if = (c, block){ bool(c, block, {})() }

loop = (n){
    done = return
    if n <= 0:
        backtrace()
        done()
    print("N:", n)
    goto loop n - 1
}
loop 10

