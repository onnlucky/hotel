block = { block -> return block }
testfunction = { ->
    return block:{-> return 42; 10000 }
}
print "BEFORE"
b = testfunction()
print "AFTER"
print "BLOCK?", b
print "VALUE", b()
print "DONE"

