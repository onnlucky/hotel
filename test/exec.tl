output = out

pid, in, out, err = %io_child_exec("echo", "hello world")
%io_close(in)
%io_close(err)

status = %io_child_wait(pid)
output("echo status: ", status, "\n")

buf = %buffer_new()
%io_read(out, buf)
%io_close(out)
output("echo stdout: ", %buffer_read_text(buf), "\n")
output("DONE\n")
