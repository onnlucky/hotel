cc, extra = continuation
out "HELLO ", extra, "\n"
done = return
bool(extra, (->done()), (->))()
cc("WORLD!")
out "ERROR", "\n"

