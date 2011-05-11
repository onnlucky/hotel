TODO
add static initializers, until first vm is created allow tSYM("...") tText("...") etc.
implement rest of vm.c to run tasks
think about how to run non primitive c functions, a stackpool?
 - maybe capture stack between begin and start when doing spagetti
how to do gc? global+train vcc + local refcounted heap + nursery

env should be mutable until closed ...
temp[] should be worker global, since they are handled atomically
remove OSETENV and OGETTEMP and such
use *parent as prototype notation (self-alike; requires ordered map or strict lexical sorting?)
implement splay: return(a1, a2, *list) by return.call(a1 :: a2 :: list)

