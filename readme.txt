TODO
rename eval to run (TRunCall, TRunBody, TRunArgs, TRun...)
rename body to code?
lazy arguments
start adding testcases
env should: know about #args, #this, #body, ... return ... etc
split evalbody in evalargs and evalcode?

do the open/close correctly; do close a lazy as possible? if x: return x ... no need to close/copy

think about how to run non primitive c functions, a stackpool?
 - maybe capture stack between begin and start when doing spagetti

how to do gc? global+train vcc + local refcounted heap + nursery

add static initializers, until first vm is created allow tSYM("...") tText("...") etc.

implement splay: return(a1, a2, *list) by return.call(a1 :: a2 :: list)

