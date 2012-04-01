x = !Task.yield; 42
print x.isDone
print x.wait

x = !Task.yield; 42
Task.yield
Task.yield
print x.isDone
print x.wait

