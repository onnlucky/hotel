if = cond, &tb, &fb -> bool(cond, bool(args.block,args.block,tb), fb)()
(->)          # captures env, but not close it
out "close\n" # close the env, but not create new one
again = c ->
    # first capture the env during bind, then set again in env ... but env is closed
    if c: return
    again(true)
again(false)
out "OK\n"

