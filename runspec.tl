_results = Array.new
var $_test = null

starttest = title ->
    $_test = title

endtest = ->
    title = $_test
    pass = _results.filter(r -> r.pass).size
    total = _results.size
    _results.each: r ->
        if not r.pass: print "FAIL:", r.name, r.msg
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
            _results.add({pass=false,{name,msg}})
        size = _results.size
        args.block.call
        if _results.size == size: # no sub tests were done, assuming this was the test
            _results.add({pass=true, {name}})
    )
    $_test = old
    if not old: endtest()

env = Env.current

var $pass = 0
var $total = 0
runtest = file ->
    starttest(file[:-4])
    catch: e -> print "ERROR RUNNING SPEC: $file", e.toString
    io.File("spec/$file").readString.eval(env)
    p, t = endtest()
    $pass += p
    $total += t

if args.size == 0:
    io.Path("spec").each: n ->
        if not n.endsWith(".tl"): continue
        runtest(n)
if args.size > 0:
    args.each: n ->
        runtest(n + ".tl")

percent = ($pass / $total * 100)
percent = { percent >= 100       }: 100
          { percent.round == 100 }: 99
          {                      }: percent.round
print "specification: $percent% pass (total: $($total) failed: $($total - $pass))"
