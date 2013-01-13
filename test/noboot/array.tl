max = 10000

ls = Array.new
x = Var.new 0
print "starting:", $x

// while is in boot.tl
loop:
    if $x >= max: break
    ls.add $x
    $x += 1
print "removing", $x, ls.size

y = Var.new 0
loop:
    if ls.size == 0: break
    ls.pop
    $y += 1

print $x, "==", $y

