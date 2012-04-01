#!/usr/bin/env tl

fail = Var.new 0
pass = Var.new 0

io.Dir(".").each: { file ->
    if not file.endsWith(".tl"): continue
    if file == "tester.tl": continue

    _catch: { e ->
        print "FAIL:", file, "ERR:", e
        $fail += 1
    }
    // TODO overwrite stdout ...
    io.File(file).read.eval
    $pass += 1
}

print "FAIL:", $fail, "PASS:", $pass

