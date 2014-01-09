p = parse("foo = 42\nprint(42.type, foo.type, (x -> x))")

emit = term ->
    { isNumber(term) }: true //print "number", term
    { isString(term) }: true //print "string", term
    { term.type == "assign" }:
        print "set", term.to
    { term.type == "function" }:
        print ">>"
        if term.as: term.as.each: arg -> print "set", arg.name
        term.body.each: stm -> emit stm
        print "<<"
    { term.type == "call" }:
        emit term.target
        term.args.each: stm -> emit stm.v
        //print "call"
    { term.type == "ref" }:
        print "lookup", term.name
    { term.type == "method" }:
        emit term.target
        //print "string", term.method
        term.args.each: stm -> emit stm.v
        //print "mcall"
    { }:
        print "OEPS"
        print inspect term
        throw "UNKNOWN TERM: $(term.type)"
emit p

