#!/usr/bin/env tl

var $fail = 0
var $pass = 0

io.Dir(".").each: file ->
    if not file.endsWith(".tl"): continue
    if file == "tester.tl": continue

    catch: e ->
        print "FAIL:", file, "ERR:", e.toString
        $fail += 1

    # TODO overwrite stdout ...
    print "TEST: $(file)"
    io.File(file).readString.eval
    $pass += 1

if $fail > 0: print "FAIL:", $fail, "PASS:", $pass; return
print "PASS:", $pass

