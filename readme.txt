# TODO
implement multiple returns
implement goto
impelment continuations

do other/better parser? or help greg along?

rename eval to run (TRunCall, TRunBody, TRunArgs, TRun...)
rename body to code or block or?
split evalbody in evalargs and evalcode?
add keyworded arguments and processing for hotel functions

lookup should handle: #this, #body, #goto and some others

do the open/close correctly; do close a lazy as possible? if x: return x ... no need to close/copy

think about how to run non primitive c functions, a stackpool?
 - maybe capture stack between begin and start when doing spagetti

how to do gc? global+train vcc + local refcounted heap + nursery

add static initializers, until first vm is created allow tSYM("...") tText("...") etc.

implement splay: return(a1, a2, *list) by return.call(a1 :: a2 :: list)


# how does the evaluator work

Basically the only thing the "bytecode" does is call functions, bind functions and collect return values.
