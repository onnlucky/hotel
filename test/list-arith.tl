list = [3,1,3,4,5,6,-8,1,2,3,4,8,9,0,3,4,5,3,2,999,1,6,7,8,5,4,3,2,1]
list.size
list.sum
list.min
list.max
list.mean
list.median
sorted = list.sort

assert(sorted.size == list.size)
assert(sorted[1] == list.min)
assert(sorted[-1] == list.max)

