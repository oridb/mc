A Toy With Delusions of Usefulness
----------------------------------

    use std
    const main = {
            var i
            for i = 0; i < 100; i++
                    std.put("You're still reading?!?\n")
            ;;
    }

Myrddin is a language that I put together for fun, but which has slowly become
a handly little language, and has attracted a number of contributors. The libraries
are written from the ground up, with zero external dependencies -- not even libc.

![Solid Engineering](http://eigenstate.org/myrddin/tacoma-narrows.jpg "Solid Engineering")

Introduction
-------------

If you want to read more about what Myrddin is or does, there's a website up.

[Myrddin Homepage](http://eigenstate.org/myrddin/)

Try It Online
-------------

Since installing a new language is a chore, there is a sandbox with code you can mess
around with. This is a very restrictive environment, but it's enough to get an idea
of what the language feels like.

[Online Playground Environment](http://eigenstate.org/myrddin/playground/)

API Documentation
-------------

Myrddin ships with a reasonably useful standard library, which covers many common uses. As
stated before, This library is implemented from scratch.

[API Reference](http://eigenstate.org/myrddin/doc/)

Supported Platforms
-------------------

Myrddin currently runs on a number of platforms

- Linux
- OSX
- FreeBSD
- 9front

Major Features
--------------

- Type inference. Types are inferred across the whole program.
- Algebraic data types.
- And their friend, pattern matching.
- Generics and traits
- A package system.
- Low level control.
- (Almost) no runtime library.
- Entirely self contained.
- Simple and easy to understand implementation.

