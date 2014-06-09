x = !Task.yield; 42
out x.isDone, "\n"
out x.wait, "\n"

x = !Task.yield; 42
Task.yield
Task.yield
out x.isDone, "\n"
out x.wait, "\n"

