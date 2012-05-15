out, status = io.run("echo hello world")
assert status == 0
assert out == "hello world\n"

(!(
    io.chdir "noboot"
    out, status = io.run("cat ../exec.tl")
    assert status == 0
    assert out.size > 100
)).wait

out, status = io.run("cat exec.tl")
assert status == 0
assert out.size > 100
