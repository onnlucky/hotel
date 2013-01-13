block = block -> return block
testfunction = ->
    b = block: => return 42; 10000
    return b
b = testfunction()
print b()
print "DONE"

