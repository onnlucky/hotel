id = a -> Task.yield; a
out id(42).round.round.round, "\n"

id = a ->
    catch: e -> a
    throw "oeps"
out id(42).round.round.round, "\n"
