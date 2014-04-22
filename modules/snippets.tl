// snippets editor support

_env = Env.current
var $env = Env.new(_env)
evalCode = code ->
    catch: e -> _snippets_print($e.toString)
    res = eval(env, code)
    if res: _snippets_print("$res")

run = ->
    loop:
        cmd, a1, a2 = _snippets_cmd()
        if cmd == null: break
        catch: e -> print e.toString
        print "snippets:", cmd, a1, a2
        { cmd == "ping" }: print "ping from snippets command!"
        { cmd == "run"  }: evalCode(a1)

{{ run }}

