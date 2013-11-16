// test if map works; have a function a -> b; generate random lists of a's verify list are equal

randomInt = t ->
    { t < 0.25 }: 2 - random(4)
    { t < 0.50 }: 100 - random(200)
    { t < 0.75 }: 2^23 - random(2^24)
    {          }: (2^29 - random(2^30)) *(2^29 - random(2^30)) * (2^29 - random(2^30)) // random doesn't work with bignum

randomSize = t ->
    { t < 0.25 }: random(4)
    { t < 0.50 }: random(33)
    { t < 0.75 }: random(100)
    {          }: random(1050)

randomList = element ->
    return (t ->
        size = randomSize(t)
        list = Array.new
        sub = t * 7 - (t * 7).floor // cycle faster through the range of our sub
        size.times: list.add(element(sub))
        return list.toList
    )

test = fn, verify, what ->
    total = 110
    total.times: t ->
        o = what(t/total)
        n = fn(o)
        verify(o, n)
    print "ran $total tests"

trans = a -> a + 1
verify = o, r -> assert o == r - 1
//test trans, verify, randomInt

map = a -> a.map(trans)
verify = o, r -> o.each: e, i -> assert r[i] == trans(e)
test map, verify, randomList(randomInt)

