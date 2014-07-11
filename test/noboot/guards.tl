test = ->
    out("start", "\n")
    { out == 42 }: return "FAIL", "BAD1"
    { false     }: return "FAIL", "BAD2"
    { 10 > 2    }: return "OK", "OK"
    {           }: return "FAIL", "BAD3"
    out("never", "\n")

a, b = test()
out(a, b, "\n")
