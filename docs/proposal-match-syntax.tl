<doc>
/author: Onne Gorter
/title: Match syntax and semantics proposal

<code>
Latch = (max ->
    guard = !(
        var $count = 0; var $holding = []

        loop: (
            msg = Task.receive; match msg[0]:
            | #enter -> (
                $count += 1
                | $count < max -> msg.reply
                | -> $hold += msg
            )
            | #exit -> (
                $count -= 1
                msg.reply
                | $holding.empty? ->
                | -> $holding[0].reply; $holding = $holding[1..]
            )
        )
    )
    (block -> guard.send #enter; block(); guard.send #exit)
)
</code>

/h1: explanation
Match cases can start with or without a `match` clause. A clause starts with a pipe (`|`). The
first clause that evaluates to true will run. An empty predicate always passes. If no clause
evaluates to true, the whole match expression will throw an error.

/h1: rationale
In the above example, we multiplex over the incoming message and state (`count` and `holding`).
The code tries to make that clear, also, unknown messages will throw errors.

/h1: weakness
The match statement itself is optional, multiple independent match statements without any
separating statements will behave as one larger, even if visually apart.

<code>
    | iseven -> print "even"
    | -> print "odd"

    // a random comment

    | frob == 42 -> print "towels"
    | hotel == "cool" -> print "really"
</code>

</doc>

