# snippets editor support

_env = Env.current
evalCode = code ->
    catch: e -> _snippets_print(e.msg)
    res = eval(Var.new(_env), code)
    _snippets_print(repr(res))

run = ->
    loop:
        cmd, a1, a2 = _snippets_cmd()
        if cmd == null: break
        catch: e -> print e.toString
        { cmd == "ping" }: print "ping from snippets command!"
        { cmd == "run"  }: evalCode(a1)

{{ run }}

