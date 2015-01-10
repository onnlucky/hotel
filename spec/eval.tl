out = Buffer.new
print = -> out.write(args.map(a -> toString(a)).join(" "), "\n"); null

var frame = true
runcode = code ->
    catch: e -> print "error: $(toString(e))"
    current = code.eval(frame=frame, {{print}})
    res = current.run
    frame = current
    if not frame.allStored and isDefined(res) and res != null: print(toString(res))

// storing values doesn't output anything
runcode "var x = 10"
runcode "foo = -> print x"
runcode "foo()"
runcode "x = 42"
runcode "foo()"

test "output":
    assert out.readString == "10\n42\n"

test "frame.get":
    assert frame.size == 2
    assert frame[1] == 42
    assert isFunction(frame[2])

    value, name, type, pos = frame.get(1)
    assert value == 42
    assert name == "x"
    assert type == "var"
    assert pos == null or isNumber(pos)

    assert frame[2].name == "foo"

// TODO better test this at some core semantics test
test "frame.get undefined is all undefined":
    value, name, type, pos, foobar = frame.get(100)
    assert isUndefined(value)
    assert isUndefined(name)
    assert isUndefined(type)
    assert isUndefined(pos)
    assert isUndefined(foobar)
