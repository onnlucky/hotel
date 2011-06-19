if = (c, block){ bool(c, block, {})() }
loop = (n){
    if n <= 0:
        backtrace
        return
    if n <= 0:
        error()
    print("N:", n)
    goto loop n - 1
}
loop 10

foo = (block){ block() }
foo(): print; print("hello"); print

print
print

