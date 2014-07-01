// run a repl, or every argument as a file
io = Env.module("io")

runmain = as ->
    catch: e -> print e.toString; _set_exitcode(1)

    print "HERE 2", as.size
    if as.size == 0: return Env.module("commandline")

    file = as[1]
    return _parse(_stringFromFile(file), file)(_env, as[2:])

main = !runmain(args)

// this will only return when there are no more tasks to run
io.ioloop

// we are done, print the results, if any, error has already been printed
if main.value: print main.value
return main.value

