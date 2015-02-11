/title: Hotel - A Programming language
/author: Onne Gorter

/h1: Overview

Hotel is a dynamic programming languages, it borrows from functional languages
and has concurrency built in. Language wise, it is inspired a lot by javascript
and ruby and a little bit by erlang and ocaml.

/h1: High Level

Hotel's main goal is to enable high level programming. Hotel is itself a high
level language, but it also enables creating "more higher" levels. It can claim
to do this because it solves a very important problem holding other languages
back: global effects. Hotel simply does not have them.

Why is this important? Any kind of global effect cannot be isolated. You can
use them inside of functions or objects, but that does not hide them (they are
global after all). It might be convenient, but it is not an abstraction, and
therefore it does not create a higher level. You still have to document the
effect, and handle that function or object as special. Effectively you must
create usage patterns around the global effects.

What are these global effects? Usually, doing io (files/network/syscalls),
defining classes, loading modules, threads (if they share), blocking, resources,
singletons and (mutable) globals.

Hotel does not permit any of these. More specifically, it allows all of them,
but in such a way they do not have global effects. Hotel only affords two
global effects:
1. computation takes time and memory;
2. blocking a computation waiting for the outside world (io).
The first is assumed to be available in abundance, and thus not a problem in
the common case. The second is a tradeoff, not allowing blocking is also a
global effect. But blocking makes io the same as a computationally heavy
algorithm, you document that fact, and allow the programmer to choose its
impact by making it easy to do the io (or heavy computation) in the background.

Note, usually (pure) functional programming is put forward as a solution to the
above problem. But it cannot be, because it does not allow you to hide the side
effects, thus the side effects become global effects. Pointed out clearly;
definitely is a good thing. But it disallows creating abstractions around side
effects, the best you can get is create conveniences.


/h1: Hotel - The Name

It is called hotel for a reason. You came here to solve your problem. Hotel,
the programming language, does the chores and housekeeping for you. So you can
focus on the problem at hand.

hotel |hōˈtel|
noun
1. an establishment providing accommodations, meals, and other services for
   travellers and tourists.
2. a code word representing the letter H, used in radio communication.
3. a programming language providing many services for programmers and
   developers.


# syntax design

Must be able to not type parens all the time. So you can skip them for the "primary" expressions and for blocks. Especially for the primary expressions, you should be able to insert a primary expression to the left of it and it should mean the same thing:

all run the initial statement unchanged
  fac 10
  print fac 10
  sometimes print fact 10

same for:
  if true: print "foo"
  timed if true: print "foo"

# selfapplication

If a name appears all by itself in single statement, it selfactivates ... the only other use for such a statement would be if it is at the end of an code block to return that value. In that case you can use return name instead.

so:
    print
would actually print a newline ...

implications, any bare statement is executed, so returning last value only works by typing:
  return value
not
  value
the latter will translate to
  value()
and fail

# about continuations and flow control

There are three fundamental control flow functions. return, goto and continuation. These always have a tRunCode as reference. Because we try to be very lazy when to materialise tRun's the host (c) stack implies the native caller flow. But non linear flow by these functions makes this a delicate situation.

# macros

Before any optimisation, macros may run. Basically after parsing, but before optimising/compiling, macros may run. As we see a macro definitions, we take it out of the source, compile it and have it defined for the current scope. As we see symbols in apply position, if they refer to a macro, we apply the macro, the arguments are unevaluated syntax tree elements.

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

The function prototype looks as follows: `void name(tlFun* fn, tlMap* args)`. Receiving the current task, a pointer back to the tlPRIM you created (from which you can retrieve the extra data elements), and a tlMap containing the arguments. You return values by tltask_return(), or tltask_throw().

There are some rules to adhere:
* don't call tlcall_call() or friends
* don't modify tlTask, tlFun or tlMap
* hotel may run using many threads concurrently, and your functions might be called on any thread, even simultaneously, even this excact same call might run concurrently (all receiving the same tlMap*).

Return only hotel values. Anything primitive can be placed under tlPointer or
others. When using your own structures, ensure to be thread safe and fully
reentrant.

Notice only true/false/null/undefined and float values are guaranteed
primitives. Especially tlNumber and tlString are tricky. They might be tlInt and
tlString*, but they might also be higher level values that act as these. So when
calling primitives you need to know they are, and force numbers and texts to be
normalised (call toPrimitive on them).

## full functions

Full functions may re-enter the evaluator. But the hotel language supports full
continuations and forks and other such constructs, and cannot therefor be
evaluated on a simple stack. So your full functions cannot use the c-level
stack, only as temporary space. Full functions must implement continuations
manually. Basically your function will get called many more times, and you have
to figure out what to do each time you are called.

to support this, your function receives and returns an extra tvalue. this value
can be anything, including your own type, as long as it adheres to how hotel
values should behave. This value is never visible at hotel level, unless you
pass it there. The first time you are passed null (not tNull).

a "simple" log function:
  tValue _log(tlFun* fn, tlMap* args, tlValue current) {
      int i = 0;
      if (current) {
          i = tl_int(current);
          // there was a continuation, so we called the evaluator, and it returned to us
          // the return value always lives in the value "register" of the current task
          tlValue v = tltask_value(task);
          tlString* s = tlString_cast(v);
          printf("%s", s?tlString_data(s):"<null>");
      }
      for (; true; i++) {
          // the iterator will return null when done
          tlValue v = tlmap_value_iter(map, i);
          if (!v) break;

          // tlvalue_toString will return null when it setup a call to the evaluator
          tlValue v = tlvalue_toString(v);
          if (!v) return tlINT(i); // return our current state
      }
      // our function returns null
      tltask_return(tlNull);
      // we return null to indicate we are fully done
      return null;
  }

Notice, these sort of functions are likely a lot easier when done as follows:

hotel:
  log = => args.map(n -> n.toString).join(" ").add("\n").toPrimitive |> output.write

## c-level task

Hotel is build using the actor model, light weight threads called a task. A task is a share nothing thread, and you push messages onto its queue, it will respond at its own leisure.

Funny thing is, this is much like an event loop, except that you write a
response back to the event. This is exactly how you implement a c level task.
Your choice is to "sacrifice" a os level thread.

The idea is simple, whenever messages became available, a callback is invoked.
This callback may signal your thread, or may run your message queue. If you
don't use your own thread, don't use any blocking calls.

# requirements

$ apt-get install ssl-dev portaudio19-dev

probably in the future

$ apt-get install libgc-dev

but for now libgc is bundled together with libatomic
