buf = _Buffer_new()
buf.write "hello"
print buf.read

/*
print "hello".size
oop = _new_object({ hello= ->{print "here"; "greetins!"} })
print oop.hello.size
/*
oop._set_ #foo, 42
//task = _new_task(->{ print "here"; "the return" })
//task.value

latch = {
    started = 0
    waiters = [].mutable
    start = ->{
        @started++
        return if @running > 5
        @waiters += Task.suspend
    }
    done = ->{
        @started--
        waiters.first.resume
    }
}

wait =-> {
    receiver = Task.new { Task.receive }
    args.each f: f.signal receiver
    receiver.value.value
}
wait(future1, future2, future3)

