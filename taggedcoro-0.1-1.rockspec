package = "TaggedCoro"
 version = "0.1-1"
 source = {
    url = "git://github.com/mascarenhas/taggedcoro.git"
}
description = {
    summary = "Tagged Coroutines",
    detailed = [[
       Enriches coroutines to have an associated tag
       (which can be any Lua value). A call to
       coroutine.yield transfers control to the nearest
       enclosing coroutine.resume with the same tag.
       Resuming that coroutine transfers control back
       to the call to coroutine.yield.
    ]],
    homepage = "https://github.com/mascarenhas/taggedcoro",
    license = "MIT/X11"
}
dependencies = {
    "lua ~> 5.3"
}
build = {
   type = "builtin",
   modules = {
     taggedcoro = "src/taggedcoro.lua",
     iterator = "contrib/iterator.lua",
     stm = "contrib/stm.lua",
     exception = "contrib/exception.lua",
     nlr = "contrib/nlr.lua",
   },
   copy_directories = {}
}
