_results = Array.new
_test = Var.new

starttest = title ->
    $_test = title

endtest = ->
    title = $_test
    pass = _results.filter(r -> not r.pass).size
    total = _results.size
    _results.each: r ->
        if not r.pass: print "fail:", r.name, r.msg
        if r.pass: print "pass:", r.name
    print "pass: $pass (of $total) for '$title'"
    _results.clear
    $_test = null
    return pass, total

test = title ->
    old = $_test
    name = { not old }: starttest(title)
           {         }: "$old - $title"
    $_test = name
    (
        catch: e ->
            msg = try(e.toString, try(e.msg), e)
            _results.add {pass=false,{name,msg}}
        size = _results.size
        args.block.call
        if _results.size == size: // no sub tests were done, assuming this was the test
            _results.add {pass=true,{name}}
    )
    $_test = old
    if not old: endtest()

env = Env.current

pass = Var.new 0
total = Var.new 0
io.Path("spec").each: n ->
    // TODO run sub directories
    if not n.endsWith(".tl"): continue
    starttest(n[:-4])
    io.File("spec/$n").readString.eval(env)
    p, t = endtest()
    $pass += p
    $total += t

percent = ($pass / $total * 100)
percent = { percent >= 100       }: 100
          { percent.round == 100 }: 99
          {                      }: percent.round
print "specification: $percent% pass (total: $($total))"
