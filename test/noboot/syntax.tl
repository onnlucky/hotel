testblock = -> out bool(args.block,"block","no block"), "\n"

// blocks
testblock: 42
testblock: -> 42
testblock: a1, a2 -> 42
testblock(-> 42)
testblock(a1, a2 -> 42)
testblock(block = -> 42)
testblock(block = a1, a2 -> 42)
fn = a1, a2 -> 42
testblock(block=fn)

// no blocks
testblock(fn)
testblock((fn))
testblock((a1, a2 -> 42))

// optional commas

out("maps\n")
res = {x = 4, y = 2}
out res.x, res.y, "\n"
res = {
    x = 4
    y = 2
}
out res.x, res.y, "\n"
res = {
    x = 4,
    y = 2
}
out res.x, res.y, "\n"
res = {
    x = 4
   ,y = 2
}
out res.x, res.y, "\n"

out("lists\n")
res = [4,2]
out res[1], res[2], "\n"
res = [
  4
  2
]
out res[1], res[2], "\n"
res = [
  4,
  2
]
out res[1], res[2], "\n"
res = [
  4
 ,2
]
out res[1], res[2], "\n"

out("functions\n")
out(4,2,"\n")
out(
  4,
  2,
  "\n"
)
out(
  4
  ,2
  ,"\n"
)
out(
  4
  2,
  "\n"
)

