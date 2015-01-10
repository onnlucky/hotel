(
    catch: e -> out(e, "\n")
    out(out(out(throw("oeps"))))
)

test = -> throw "oeps"
(
    catch: e -> out(e, "\n")
    test(test(test(test())))
)

test2 = -> test()
(
    catch: e -> out(e, "\n")
    test2()
)

a = Array.new
a.add(1)

catch: e -> out(e, "\n")
_with_lock(a): test2()


