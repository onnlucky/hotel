out, status = io.Process.run("echo hello world")
assert status == 0
assert out == "hello world\n"

