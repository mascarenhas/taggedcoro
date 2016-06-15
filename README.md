# Tagged Coroutines

This module presents an interface identical to the standard
"coroutine" module, but adds a *tag* parameter to `create`,
`wrap`, `yield`, and `isyieldable`. The tag can be any
Lua value.

A yield with a specific tag yields to the dynamically
closest resume of a coroutine with that tag (making
an analogy with exceptions, the resume is analogous
with a try/catch block, yield is analogous to a throw,
and the tag is analogous with the type of the exception).

Any coroutine that yield passes through in the search
of the coroutine that will handle that yield becomes
*stacked*, a new status (returned by `status`).
Stacked coroutines cannot be
resumed. Resuming the coroutine that received the yield
rewinds the whole stack, resuming the stacked coroutines
along the way until resuming from the point of the original
yield.

The `isyieldable` function now expects a tag, and
returns `true` if yielding with that tag will be caught
by any currently active coroutine.

Install it by running `luarocks make` on the provided
rockspec file. The `contrib` folder has some libraries
that implement some abstractions on top of coroutines that
can be freely composed with tagged coroutines. The
`samples` folder has sample scripts that exercise
these higher-level libraries. Some of them depend on
the [thread](https://github.com/mascarenhas/thread)
library and on a branch of [Cosmo](https://github.com/mascarenhas/cosmo/tree/taggedcoro)
that requires tagged coroutines.

