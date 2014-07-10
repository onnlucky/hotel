// validate numbers according to luhns checksum, for creditcards and such
luhn = digits ->
    normalized = digits.map(l -> Number(l)).filter.reverse

    sum = Var.new 0
    normalized.each: e, n ->
        { n % 2 == 0 }: $sum += (e * 2) /. 10 + (e * 2) % 10
        {            }: $sum += e
    return $sum % 10 == 0

assert luhn("5248 8602 0036 8643")
assert luhn("79927398713")
assert not luhn("79927398710")
assert not luhn("79927398711")
assert not luhn("79927398712")
assert not luhn("79927398714")
assert not luhn("79927398715")
assert not luhn("79927398716")
assert not luhn("79927398717")
assert not luhn("79927398718")
assert not luhn("79927398719")

