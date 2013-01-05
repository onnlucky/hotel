out("step 1\n")
(
    catch: out("ok\n")
    assert("oeps" == "no way")
    out("fail\n")
)
out("step 2\n")
(
    assert(not "oeps" == "no way")
)
out("step 3\n")
(
    assert("must be unequal", "oeps" != "no way")
)
out("step 4\n")
(
    catch: out("ok\n")
    assert("last", "is", "false", "so", "fail", false)
)
out("done\n")
