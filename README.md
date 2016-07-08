[![Build Status](https://travis-ci.org/mascarenhas/taggedcoro.svg?branch=master)](https://travis-ci.org/mascarenhas/taggedcoro)
[![Coverage Status](https://coveralls.io/repos/github/mascarenhas/taggedcoro/badge.svg?branch=master)](https://coveralls.io/github/mascarenhas/taggedcoro?branch=master)

# [Tagged Coroutines 1.0.0](http://mascarenhas.github.io/taggedcoro/)

This module is is a replacement to the standard `coroutine`
module that adds *tagged* coroutines. Functions `create`
and `wrap` now receive a *tag* and a function, instead
of just a function. Function *yield* now also needs a
tag as the first argument. The tag can be any Lua value.

A `yield` with a specific tag yields to the dynamically
closest `resume` of a coroutine with that tag (making
an analogy with exceptions: the coroutine is like
a try/catch block, `yield` is like a throw,
and the tag is analogous with the type of the exception).
If there is no coroutine with the tag an error is thrown
at the point of the `yield`.

On a successful yield, any coroutine that has been passed
through in the search for the coroutine that handled that
yield becomes `stacked`, a new status string returned
by the `status` function. Attempting to directly resume a
stacked coroutine is an error. Resuming the coroutine that
handled the yield rewinds the whole stack, resuming the
stacked coroutines along the way until reaching and finally
continuing from the point of the original yield.

A failed yield can be an expensive operation, so if you are
unsure if you can yield you can use the extended `isyieldable`
function, which now expects a tag and will return `true`
only if yielding with this tag will succeed.

The function `coroutine.yield` is an *untagged* yield. A tagged
coroutine passes an untagged yield along, unless its parent
is the main coroutine: in this case, the yield is supressed, and
the source resumes from the untagged yield with the call to yield
returning `nil` and `"untagged coroutine not found"`. Unfortunately
there is no way to make the untagged yield fail as if it had tried
to yield outside a coroutine.

When an untagged yield reaches an untagged parent, the parent will
suspend as if the yield was intended for it; when the parent
resumes the whole stack will be resumed, ultimately resuming
from the point of the untagged yield. This way you can
have a stack of tagged coroutines on top of an untagged coroutine
(allowing the use of existing coroutine schedulers, for example).

A tagged yield that reaches an untagged coroutine fails at the
point of the call to `yield` as if it had reached the main coroutine.

A new function `call` resumes a coroutine as if it had been
*wrapped* by `wrap`: any uncaught errors while running the
coroutine will be propagated. But the stack is not unwound:
you can still get a traceback of the full stack of the dead coroutine
(including all of the coroutines that were stacked above it) using
the new `traceback` function. It is similar to `debug.traceback`,
except that it includes a full traceback, following `source` to
reach the source of the error and tracing `parent` back to the main
thread.

A new `tag` function returns the tag of a coroutine. A `parent`
function returns the coroutine that last resumed a coroutine.
A `source` function returns, for a given coroutine,
either the coroutine where the last `yield` came from,
in case of a `suspended` coroutine, or
where an error originated, in case of a `dead` coroutine. You can
use these two functions to walk a `dead` stack of coroutines
with the `debug` functions in case `traceback` is not enough.

Finally, the function `fortag` receives a tag and returns a
set of tagged coroutine functions specialized for that tag.
For compatibility with [lua-coronest](https://github.com/saucisson/lua-coronest)
there is also a `make` function that is like `fortag` except it
generates a fresh tag if none is given.

There is both a C and a pure Lua implementation. The C
implementation is more efficient, and produces better
stacktraces, but requires stock Lua 5.2 or higher (it
will not work with LuaJIT 2). The pure Lua implementation
should work on LuaJIT 2, Lua 5.2, or Lua 5.3,
but the `isyieldable` might give a false positive if
there are pending unyieldable C calls in the stack on any
Lua version except Lua 5.3.

Install it by running `luarocks make` on one of the provided
rockspec files (`taggedcoro` for the C version, `taggedcoro-purelua`
for the Lua version). The `contrib` folder has sample libraries
that implement some abstractions on top of coroutines that
can be freely composed with tagged coroutines. The
`samples` folder has sample scripts that exercise
these higher-level libraries. Some of them depend on
the [thread](https://github.com/mascarenhas/thread)
library and on a branch of [Cosmo](https://github.com/mascarenhas/cosmo/tree/taggedcoro)
that requires tagged coroutines.
