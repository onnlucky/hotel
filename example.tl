Queue = {
    new: (depth ->
        queue = MsgQueue.new
        running = Var.new 0
        holding = Array.new
        !(
            loop:
                msg = queue.get
                | msg.name == "put" |
                    | $running < depth | $running += 1; msg.reply
                    | true             | holding.add msg
                | msg.name == "get" |
                    | holding.size > 0 | msg.reply; holding.remove(1).reply
                    | true             | $running -= 1; msg.reply
        )
        return queue.input
    )
}

throttle = (
    q = Queue.new 1
    (block -> q.put; r = block(); q.get; return r)
)

urls = ["http://google.com", "http://onnlucky.com", "http://slashdot.org"]
urls.map(url -> !throttle: http.get url).each: t -> print t.value[:100].replace("\n", " ")
print "done"

