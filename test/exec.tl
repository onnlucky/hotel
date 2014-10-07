out, status = io.run("echo hello world")
assert status == 0
assert out == "hello world\n"

(!(
    io.chdir("noboot")
    out, status = io.run("cat ../exec.tl")
    assert status == 0
    assert out.size > 100
)).wait

(
    catch: e -> true
    io.run("false")
    assert false
)
(
    catch: e -> true
    io.run("false", allowerror=false)
    assert false
)

(
    catch: e -> assert false
    io.run("false", allowerror=true)
)

io.shell("echo 42 > /tmp/testing")
assert io.File("/tmp/testing").read == "42\n"
io.shell("rm -rf /tmp/testing")

out, status = io.run("cat exec.tl")
assert status == 0
assert out.size > 100
