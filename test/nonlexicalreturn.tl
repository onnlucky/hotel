block = { block -> return block }
testfunction = { ->
    return block:{-> return 42; 10000 }
}
b = testfunction()
print b()
print "DONE"

