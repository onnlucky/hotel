test "goto doesn't add stacks":
    current = Task.stacktrace.size
    test = n ->
        { n > 0  }: print goto test(n - 1)
        {        }: null
        assert Task.stacktrace.size - current == 1
    test(0)
    test(1)
    test(10)
    test(100)

test "goto can return value's":
    foo = -> goto 1
    assert foo() == 1

test "goto can must take single arg":
    var x = false
    foo = ->
        catch: e -> x = true
        goto 1, 1
    foo()
    assert x

