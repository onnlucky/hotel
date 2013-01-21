TEST = "hello"
REF = "ell"
//TEST = [1, 2, 3, 4, 5]
//REF = [2, 3, 4]

assert TEST[:] == TEST
assert TEST[1:] == TEST
assert TEST[:-1] == TEST
assert TEST[1:-1] == TEST
assert TEST[2:-2] == REF

print 0, TEST[0:0]
print 1, TEST[1:1]
print 4, TEST[4:4]
print 5, TEST[5:5]
print 6, TEST[6:6]
print -1, TEST[-1:-1]
print -2, TEST[-2:-2]

print TEST.get
print TEST[0]
print TEST[1]
print TEST[2]

print TEST.startsWith("hello")
print TEST.startsWith("ell", 2)
print TEST.startsWith("llo", -3)

