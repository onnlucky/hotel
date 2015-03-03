Hotel - A Programming language
------------------------------

author: Onne Gorter

Hotel is a dynamic programming languages, it borrows from functional languages
and has concurrency built in. Language wise, it is inspired a lot by javascript
and ruby and a little bit by erlang and ocaml.

High Level
----------

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


Mini Tutorial
------------
```
a = "hello"
b = "world"
print(a, b)
```
Outputs "hello world" on the console, `a` and `b` are not so much variables, but
immutable name bindings. Hotel supports mutations, but you have to ask.

```
var x = 0
"hello world".split.each: word ->
  x += word.size
print x
```
Outputs 10. While not a bad solution, it can also be written without:
```
"hello world".split.map(word -> word.size).sum
```
The choice is yours. But hotels default choices are immutable. Like in this
example, `String.split` returns a `List` which is an immutable. Much like
`Strings` in most languages.

As a last example:
```
page = html.parse(http.get("http://en.wikipedia.org/wiki/Hotel"))
print(page.find("p").text)
```
Which will print the first page of the wikipedia article on hotels. (Not this
language, haha).


Status
------

Hotel is in early stages. It is still missing real classes (can be simulated),
and many other features and things you'd expect.

Also hotel has no illusion of being fast.


Hotel - The Name
----------------

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


Building
--------

requires libgc (aka boehmgc or bdw-gc)
optionally libopenssl, portaudio, cairo + libjpeg
and for a graphics environment, either gtk or cocoa

```
make
sudo make PREFIX=/usr install
```

That will install a `tl` command, the main interpreter. Also a `gtl` command,
that last one is for opening windows in a desktop environemnt.


Options to make are: `NO_PORTAUDIO=1`, `NO_CAIRO=1`,  `NO_OPENSSL=1`,
`NO_GRAPHICS=1` to disable building/installing modules.

a headless install:
```
make NO_PORTAUDIO=1 NO_GRAPHICS=1
sudo make NO_PORTAUDIO=1 NO_GRAPHICS=1 PREFIX=/usr install
```


License
-------

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>

