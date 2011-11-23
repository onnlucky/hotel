queue = MsgQueue.new
t1 = Task.current
t2 = !t1.wait
Task.yield
print "should deadlock", t1, t2
t2.wait

