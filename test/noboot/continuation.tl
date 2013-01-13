cc, extra = continuation
print "HELLO", extra
done = return
bool(extra, (->done()), (->))()
cc("WORLD!")
print("ERROR")

