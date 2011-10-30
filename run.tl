child = _Child_run("echo", "hello", "world")
child.in.close
child.err.close

print "here"
//status = child.wait
//out("echo status: ", status, "\n")

buf = _Buffer_new()
child.out.read(buf)

out("echo stdout: ", buf.read, "\n")
out("DONE\n")

