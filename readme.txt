# TODO

rework all types as the tlClass things ... stop using anything else
rename/rework style: tlArgsGet(args, 0), tlArgsTarget(args), tlArgsMsg(args), tlMapGet(wr, map, key)
we do the "get field and dispatch" a lot in a lot of places ... dry it up

add send as code primitive: target, msg, args, instead of _object_send
add op as code primitive: op, lhs, rhs

think about evio doing read/write simultaniously ... two actors?
writing to sockets should accept strings too ... place buffer inbetween? actor inside/outside?

add methods vs functions, methods try to bind a this dynamically ... helps with actors too
example: function = { }
         method = @{ }

  ----

ensure task attach/detach is correct, and ready/done etc don't have races ...

fix task_alloc, rename to value_alloc, use sizeof(impl) not field count
throw exceptions a little bit better, at least cat texts with message ...
clean up task.c arrange things around tlResult and such ...
clean up tl.h; nice up code.{h,c} and call.c

fix return/goto when target has returned already ... what to do anyhow?

expose parsed code: tlBlock, tlCall, tlAssign, tlLookup? or not?
only optimize after parser, inspect step: single tlAssign become just name ...

add a syntax for blocks like `catch: e -> print exception` and `arr.each(e -> print e)`

sprinkle more INTERNAL around and such
think about c-stack eval until we need to pause, limit its depth somehow
remove start_args ... we don't need to materialize its run all the time

add default arguments using print = (sep=" ", end="\n")->{...} etc ...
implement collector: x, *rest = multiple_return()
implement splay: return(a1, a2, *list) by return.call(a1 :: a2 :: list)
implement lvalue assignment: mutable.field = fn()

implement defer (add defer[] to tlCodeRun) or something ...

bring back a boot.tl library
implement serializing tlValue's to disk

experiment with refcounting, experiment with alloc pool per task for certain sizes ...
* alloc ref=1
* when ref > 1, assert that all fields are valid
* when doing mutable things, assert that ref == 1, copy/clone otherwise
* think about how to defer refcounting to battle "churn"
x = KEEP(v); return FREE(v); return PASS(v);

optimize: parser should add all local names to code->envnames and env should use this ...
optimize: parser pexpr and others lots of branches start out the same, let them share prefix ...
optimize: task->value by tagging as active incase of tResult or such?
optimize: compile code by collecting all local names, use that as dict, and keep values inside run

do the open/close correctly; do close as lazy as possible? if x: return x ... no need to close/copy

GC:
* first, boehm will suffice
* local refcounted/mutable heap with tracing collector
* global heap = immutable, non-cyclic, or it can be, tasks can be mutable global dict

add static initializers, until first vm is created allow tlSYM("...") tlText("...") etc.


# syntax design

Must be able to not type parens all the time. So you kan skip them for the "primary" expressions and for blocks. Especially for the primary expressions, you should be able to insert a primary expression to the left of it and it should mean the same thing:

all run the initial statment unchanged
  fac 10
  print fac 10
  sometimes print fact 10

same for:
  if true: print "foo"
  timed if true: print "foo"

# selfapplication

If a name appears all by itself in single statement, it selfactivates ... the only other use for such a statment would be if it is at the end of an code block to return that value. In that case you can use return name instead.

so:
    print
would actually print a newline ...

implications, any bare statment is executed, so returning last value only works by typing:
  return value
not
  value
the latter will translate to
  value()
and fail

# about continuations and flow control

There are three fundamental control flow functions. return, goto and continuation. These always have a tRunCode as reference. Because we try to be very lazy when to materialize tRun's the host (c) stack implies the native caller flow. But non linear flow by these functions makes this a delicate situation.

# macros

Before any optimization, macros may run. Basically after parsing, but before optimizing/compiling, macros may run. As we see a macro definitions, we take it out of the source, compile it and have it defined for the current scope. As we see symbols in apply position, if they refer to a macro, we apply the macro, the arguments are unevaluated syntax tree elements.

macro repeat = () {
    if args.size > 0: throw "repeat expects a single block"
    if !args.block: throw "repeat expects a block"
    args.block.add-first parse "again=continuation"
}

buffer = Buffer.new
file = Path("index.html").open
repeat:
    len = file.read buffer
    if len > 0: again

# how does the evaluator work

Basically the only thing the "bytecode" does is call functions, bind functions and collect return values.

# native functions

When you wish to extend hotel with c level function, you have four options:
1. implement primitive functions
2. implement functions that can re-enter the evaluator before being done
3. implement an actor

## primitive functions

Primitive functions may only return hotel values, or exceptions. They cannot
call the evaluator. But they are by far the easiest to understand.

In any scope, you register a tPRIM under a name, the tPRIM will need a function
pointer. Optionally you can add more data elements.

The function prototype looks as follows: `void name(tlTask* task, tlFun* fn, tlMap* args)`. Receiving the current task, a pointer back to the tlPRIM you created (from which you can retrieve the extra data elements), and a tlMap containing the arguments. You return values by tltask_return(), or tltask_throw().

There are some rules to adhere:
* don't call tlcall_call() or friends
* don't modify tlTask, tlFun or tlMap
* hotel may run using many threads concurrently, and your functions might be called on any thread, even simultaniously, even this excact same call might run concurrently (all receiving the same tlMap*).

Return only hotel values. Anything primitive can be placed under tlPointer or
others. When using your own structures, ensure to be thread safe and fully
reentrant.

Notice only true/false/null/undefined and float values are guarenteed
primitives. Especially tlNumber and tlText are tricky. They might be tlInt and
tlText*, but they might also be higher level values that act as these. So when
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
  tValue _log(tlTask* task, tlFun* fn, tlMap* args, tlValue current) {
      int i = 0;
      if (current) {
          i = tl_int(current);
          // there was a continuation, so we called the evaluator, and it returned to us
          // the return value always lives in the value "register" of the current task
          tlValue v = tltask_value(task);
          tlText* s = tltext_cast(v);
          printf("%s", s?tltext_data(s):"<null>");
      }
      for (; true; i++) {
          // the iterator will return null when done
          tlValue v = tlmap_value_iter(map, i);
          if (!v) break;

          // tlvalue_toText will return null when it setup a call to the evaluator
          tlValue v = tlvalue_toText(task, v);
          if (!v) return tlINT(i); // return our current state
      }
      // our function returns null
      tltask_return(tlNull);
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

