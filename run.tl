var fuut
test = -> assert isFunction fuut
print {{ test }}
fuut = -> print "OK"
test()
