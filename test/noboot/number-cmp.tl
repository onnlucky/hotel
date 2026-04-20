# Test <= and > operator methods directly; the || tlEqual bug makes them always return true/false
lte = Object.get(Int._methods, "<=")
gt = Object.get(Int._methods, ">")

out(lte(lhs=5, rhs=10), "\n")   # true:  5 <= 10
out(lte(lhs=10, rhs=5), "\n")   # false: 10 > 5
out(lte(lhs=5, rhs=5), "\n")    # true:  equal

out(gt(lhs=10, rhs=5), "\n")    # true:  10 > 5
out(gt(lhs=5, rhs=10), "\n")    # false: 5 < 10
out(gt(lhs=5, rhs=5), "\n")     # false: equal
