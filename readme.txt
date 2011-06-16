# TODO
sprinkle more INTERNAL around and such
remove start_args ... we don't need to materialize its run all the time
clean up tl.h; nice up code.{h,c} and call.c

add more arg processing:
* no keys in -> no names + defaults in target
* no keys in -> names + defaults in target
* keys in -> no names in target
* keys in -> names + defaults in target
do collector and splays ...

implement goto
implement blocks
implement tasks and message sending primitives
implement exceptions
implement continuations
implement keyworded argument passing into function
implement default arguments for function
implement defer

bring back a boot.tl library
implement serializing tValue's to disk

experiment with refcounting, experiment with alloc pool per task for certain sizes ...
* alloc ref=1
* when ref > 1, assert that all fields are valid
* when doing mutable things, assert that ref == 1, copy/clone otherwise
* think about how to defer refcounting to battle "churn"

optimize: return should not capture run, but run->caller (or not?)
optimize: task->value by tagging as active incase of tResult or such?
optimize: compile code by collecting all local names, use that as dict, and keep values inside run

lookup should handle: #this, #body, #goto and some others

do the open/close correctly; do close as lazy as possible? if x: return x ... no need to close/copy

how to do gc? global+train vcc + local refcounted heap + nursery

add static initializers, until first vm is created allow tSYM("...") tText("...") etc.

implement splay: return(a1, a2, *list) by return.call(a1 :: a2 :: list)


# how does the evaluator work

Basically the only thing the "bytecode" does is call functions, bind functions and collect return values.

# native functions

When you wish to extend hotel with c level function, you have four options:
1. implement primitive functions
2. implement functions that can re-enter the evaluator before being done
3. implement a task in c
4. implement a remote task

## primitive functions

Primitive functions may only return hotel values, or exceptions. They cannot
call the evaluator. But they are by far the easiest to understand.

In any scope, you register a tPRIM under a name, the tPRIM will need a function
pointer. Optionally you can add more data elements.

The function prototype looks as follows: `void name(tTask* task, tFun* fn, tMap* args)`. Receiving the current task, a pointer back to the tPRIM you created (from which you can retrieve the extra data elements), and a tMap containing the arguments. You return values by ttask_return(), or ttask_throw().

There are some rules to adhere:
* don't call tcall_call() or friends
* don't modify tTask, tFun or tMap
* hotel may run using many threads concurrently, and your functions might be called on any thread, even simultaniously, even this excact same call might run concurrently (all receiving the same tMap*).

Return only hotel values. Anything primitive can be placed under tPointer or
others. When using your own structures, ensure to be thread safe and fully
reentrant.

Notice only true/false/null/undefined and float values are guarenteed
primitives. Especially tNumber and tText are tricky. They might be tInt and
tText*, but they might also be higher level values that act as these. So when
calling primitives you need to know they are, and force numbers and texts to be
normalized (call toPrimitive on them).

## full functions

Full functions may re-enter the evaluator. But the hotel language supports full
continuations and forks and other such constructs, and cannot therefor be
evaulated on a simple stack. So your full functions cannot use the c-level
stack, only as temporary space. Full functions must implement continuations
manually. Basically your function will get called many more times, and you have
to figure out what to do each time you are called.

to support this, your function receives and returns an extra tvalue. this value
can be anything, including your own type, as long as it adheres to how hotel
values should behave. This value is never visible at hotel level, unless you
pass it there. The first time you are passed null (not tNull).

a "simple" log function:
  tValue _log(tTask* task, tFun* fn, tMap* args, tValue current) {
      int i = 0;
      if (current) {
          i = t_int(current);
          // there was a continuation, so we called the evaluator, and it returned to us
          // the return value always lives in the value "register" of the current task
          tValue v = ttask_value(task);
          tText* s = ttext_cast(v);
          printf("%s", s?ttext_data(s):"<null>");
      }
      for (; true; i++) {
          // the iterator will return null when done
          tValue v = tmap_value_iter(map, i);
          if (!v) break;

          // tvalue_toText will return null when it setup a call to the evaluator
          tValue v = tvalue_toText(task, v);
          if (!v) return tINT(i); // return our current state
      }
      // our function returns null
      ttask_return(tNull);
      // we return null to indicate we are fully done
      return null;
  }

Notice, these sort of functions are likely a lot easier when done as follows:

hotel:
  log = => args.map(n -> n.toText).join(" ").add("\n").toPrimitive |> output.write

## c-level task

Hotel is build using the actor model, light weight threads called a task. A task is a share nothing thread, and you push messages onto its queue, it will respond at its own leisure.

Funny thing is, this is much like an event loop, except that you write a
response back to the event. This is excatly how you implement a c level task.
Your choice is to "sacrifice" a os level thread.

The idea is simple, whenever messages became available, a callback is invoked.
This callback may signal your thread, or may run your message queue. If you
don't use your own thread, don't use any blocking calls.

