A Toy With Delusions of Usefulness
----------------------------------

    use std
    const main = {
            var i
            for i = 0; i < 100; i++
                    std.put("You're still reading?!?\n")
            ;;
    }

Myrddin is a language that I put together for fun, but which has developed
delusions of usefulness. It's made by one coder with too little spare time on
his hands.

It is designed to be a simple programming language that runs close to the
metal, giving the programmer predictable and transparent behavior and mental
model. It also attempts to strong type checking, generics, type inference, and
other features not present in C.

Myrddin is not a language designed to explore the forefront of type theory or
compiler technology. It is not a language that is focused on guaranteeing
perfect safety. It is satisfied to be a practical, small, fairly well defined,
and easy to understand language for code that needs to be close to the
hardware.

In order to run, Myrddin currently requires an x86-64 computer running either
Linux or OSX. Porting to other Posix-like OSes should be a matter of writing
syscall wrappers.

Why Myrddin?
------------

Are you tired of your compiler Just Working? Are you tired of having your
standard library having the basic functionality you need? Do you long for the
days when men were men, and debuggers operated on the assembly level? If so,
Myrddin is for you.

![Solid Engineering](http://eigenstate.org/myrddin/tacoma-narrows.jpg "Solid Engineering")

More seriously, Myrddin has the goal of replacing C in its niche of OS and
embedded development, but making it harder to shoot yourself in the foot. It's
a language that matches the author's taste in design, and if other people want
to use it, so much the better.

Major Features
--------------

- Type inference. Types are inferred across the whole program.
- Algebraic data types.
- And their friend, pattern matching.
- Generics.
- A package system.
- Low level control.
- (Almost) no runtime library.
- Entirely self contained.
- Simple and easy to understand implementation.

Try It Online
-------------

If you want to try before you buy, there is an online playground that you can
use to experiment with code. It's fairly restrictive, and prevents you from
using the vast majority of system calls, but it's enough to get a feel for
things.

[So, give it a shot.](http://eigenstate.org/myrddin/playground)
