if = cond, &tb, &fb -> bool(cond, bool(args.block,args.block,tb), fb)()
max = 10000

ls = Array.new
var $x = 0
out "starting: ", $x, "\n"

# while is in boot.tl
loop:
    if $x >= max: break
    ls.add($x)
    $x += 1
out "removing ", $x, " ", ls.size, "\n"

var $y = 0
loop:
    if ls.size == 0: break
    ls.pop
    $y += 1

out $x, " == ", $y, "\n"

