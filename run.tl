loop = (n){
    done = return;
    bool(n <= 0,{backtrace(); done()}, {})()
    print("N:", n);
    goto loop n - 1
}
loop 10

