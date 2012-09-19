{->}  // captures env, but not close it
print // close the env, but not create new one
again = { c ->
    // capture closed env, then setting again, copy-on-writes a new env, ... but we captured the old ... oeps
    if c: return
    again(true)
}
again(false)
print "OK"

