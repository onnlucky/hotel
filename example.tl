Queue = {
    new: (depth ->
        queue = MsgQueue.new
        var $running = 0
        holding = Array.new
        !(
            loop:
                msg = queue.get
                | msg.name == "put" |
                    | $running < depth | $running += 1; msg.reply
                    |                  | holding.add msg
                | msg.name == "get" |
                    | holding.size > 0 | msg.reply; holding.remove(1).reply
                    |                  | $running -= 1; msg.reply
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

