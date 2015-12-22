Myrddin
-------

Myrddin is a systems language that is both powerful and fun to use.
It aims for C like low level control, a lightweight high quality implementation,
and features you may find familiar from languages like like rust and ocaml.

This combination makes Myrddin suitable for anything ranging from desktop
applications, to embedded systems and potentially even kernel development.

Examples
--------

A classic:
```
use std

const main = {

	for var i = 0; i < 1000; i++
		/* pattern match on a tuple */
		match (i % 3, i % 5)
		| (0, 0): std.put("fizzbuzz\n")
		| (0, _): std.put("fizz\n")
		| (_, 0): std.put("buzz\n")
		| _:
		;;
	;;
}
```
How about regex, destructuring and algebraic data types?
```
use regex
use std

const main = {
	var re, str

	str = "match against this!"
	match regex.compile(".*")
	| `std.Ok r:	re = r
	| `std.Fail m:	std.fatal("couldn't compile regex: {}\n", m)
	;;
	match regex.exec(re, str)
	| `std.Some _:  std.put("regex matched\n")
	| `std.None:	std.fatal("regex did not match\n")
	;;
	regex.free(re)
}
```

More examples and a complete feature list can be found on the website.

Status
------

![Solid Engineering](http://eigenstate.org/myrddin/tacoma-narrows.jpg "Solid Engineering")


Website
-------

[Myrddin Homepage](http://eigenstate.org/myrddin/)

Try It Online
-------------

The online playground is a good place to get started with little setup.

[Online Playground Environment](http://eigenstate.org/myrddin/playground/)

API Documentation
-------------

Myrddin ships with standard library which covers many common uses. It is becoming
more useful every day.

[API Reference](http://eigenstate.org/myrddin/doc/)

Mailing List
-------------

Annoucements of major changes, questions, complaints. We also give relationship advice.

[Mailing List Archives](http://eigenstate.org/archive/myrddin-dev/)

[Subscribe Here](http://eigenstate.org/myrddin/list-subscribe)

Supported Platforms
-------------------

Myrddin currently runs on a number of platforms

- Linux
- OSX
- FreeBSD
- 9front

