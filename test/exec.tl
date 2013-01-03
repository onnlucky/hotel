out, status = io.run("echo hello world")
assert status == 0
assert out == "hello world\n"

(!(
    io.chdir "noboot"
    out, status = io.run("cat ../exec.tl")
    assert status == 0
    assert out.size > 100
)).wait

(
    _catch: e -> true
    io.run "false"
    assert false
)
(
    _catch: e -> true
    io.run "false", allowerror=false
    assert false
)

(
    _catch: e -> assert false
    io.run "false", allowerror=true
)

out, status = io.run("cat exec.tl")
assert status == 0
assert out.size > 100
