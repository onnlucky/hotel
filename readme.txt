TODO
process incoming arguments
rename eval to run (TRunCall, TRunBody, TRunArgs, TRun...)
do the open/close correctly; do close a lazy as possible? if x: return x ... no need to close/copy


think about how to run non primitive c functions, a stackpool?
 - maybe capture stack between begin and start when doing spagetti

how to do gc? global+train vcc + local refcounted heap + nursery

add static initializers, until first vm is created allow tSYM("...") tText("...") etc.

implement splay: return(a1, a2, *list) by return.call(a1 :: a2 :: list)

