# TODO

now that we unwind all stack frames, continuations must work differently ...

implement print in user code, or have it call toText properly ...
do errors with nice backtraces per file etc, and per task ...

add a "static" layer, where the symbol table, gc, and mainloops+locks live
allow multiple vm's per loop, and muliple loops per vm (io and gui loops...) handle when to exit...
what about close() and stat(); they might still block or eintr ... do them on yet another thread?

bug: tlCallableIs does not know complex user objects, just try and catch not callable?
bug: { x: 42, x: runtime } is error due to duplicate x

add {{ foo }} as sugar for { foo: foo } so modules just do {{ publicFn1, publicFn2, ... }}
do private using $: { public: 42, $private: -1 } accessed just by $private ...
use * for vars; like this: x = var 0; *x += 1 ... add .increment/decrement and such to var
syntax change: do symbols using 'symbol

fix return from blocks, how about -> for block => for function wich allows return?
Or/and break with value ... ?

add @method arg, arg
instead of hotel, lets call it arrows? .rr? .arrow? sounds nice

start preparing a first release:
* stamp every file with license/author
* use gcov to remove any unused code or add tests for them (larger parts...)
* clean up and comment eval.c maybe remove some of it to run.c oid

think about special inherited task local *Env*:
* for stdin/stdout/stderr
* for cwd
* for module resolving path
* for error mode (report on stdout or throw on waiter/value)

module lookup: should be in a task, should try sender path, then all module paths
but module lookup should not be automatic but maybe sys.io.chdir

add send as code primitive: target, msg, args, instead of _object_send
add op as code primitive: op, lhs, rhs

add methods vs functions, methods try to bind a this dynamically ... helps with actors too
example: function = ( -> ); method = ( @, -> )

implement `Point = { recurse: Point }` for as far as we can? do Point lazily?
add task.stop to kill it by error? java ThreadDeath how do we do it safely?
add finalizers to tasks: report errors if nobody else reads them
add finalizers to opened files: closed the fds

add default arguments using print = (sep=" ", end="\n")->{...} etc ...
implement collector: x, *rest = multiple_return()
implement splay: return(a1, a2, *list) by return.call(a1 :: a2 :: list)
implement lvalue assignment: mutable.field = fn()

implement defer (add defer[] to tlCodeRun) or something ...

implement serializing tlValue's to disk

optimize: compiler should add all local names to code->envnames and env should use this ...
optimize: compiler can shortcut writing/referencing local names to just indexes into locals.
optimize: remove tlHead in favor of just a tlClass ... use last 3 bits as flags
optimize: tlFrame can use 3 bits from resumecb too ...
optimize: tlArgs (and tlCall) can be reworked more lean and simpler

# missing

operators (well, the real ones)
classes (well, syntax for them and super and such ...)
class loaders for overriding/extending known classes
modules
c-based modules
lazy evaluation (call by need)
finalizers
default arguments
splays on either side
complex lhs assigns
syntax for branches:
  | false -> ...
  | true -> ...

# not so nice:

args.block(42) will not execute block ... it will ignore the fact block is callable
args.block.call(42) will ignore param; why actually?
@ is not a word ... for method(42, this=something)
block: (arg ->
    return // returns way too much, should return block ... do => for functions, -> for block?
)

# hotel - a programming language

Hotel is a programming language. Its main focus is to make everything in the language highlevel,
firstclass and simple. If the language can do something for you, you don't have to. In its design
it is heavily influenced by the "scripting" languages: javascript, ruby, etc. but different:
1. The default is immutable;
2. modules and environments can be mixed and matched;
3. there are no threads, instead there are tasks and actors.

Why go throught all that trouble? Because this way, regardless of any design decision library writers make, you can build your applications the way it bests suits you. Even in the face of blocking APIs or callback based APIs or library dependecies mutually exclusive to yours or to other libraries.

It is called hotel for a reason. You came here to solve your problem. Hotel does the chores and is here to service you.

hotel |hōˈtel|
noun
1 an establishment providing accommodations, meals, and other services for travelers and tourists.
2 a code word representing the letter H, used in radio communication.
3 a programming language providing many services for programmers and developers.


# syntax design

Must be able to not type parens all the time. So you kan skip them for the "primary" expressions and for blocks. Especially for the primary expressions, you should be able to insert a primary expression to the left of it and it should mean the same thing:

all run the initial statment unchanged
  fac 10
  print fac 10
  sometimes print fact 10

same for: (impossible! how did parser know true is not a function? ... well true is the simple case)
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

