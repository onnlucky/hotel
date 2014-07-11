test "iterate":
    [a=42,b=100].each: k, v, n ->
        assert n == 1 or n == 2
        assert k == "a" or k == "b"
        assert v == 42 or v == 100

