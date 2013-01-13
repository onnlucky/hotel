(->)          // captures env, but not close it
print "close" // close the env, but not create new one
again = c ->
    // first capture the env during bind, then set again in env ... but env is closed
    if c: return
    again(true)
again(false)
print "OK"

