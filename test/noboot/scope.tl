oldout = out
foo = msg -> oldout(msg)
out = 42
foo("hello world!\n")
