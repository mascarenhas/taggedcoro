
local coroutine = require "taggedcoro"

local iterator = { TAG = "iterator" }

function iterator.make(f)
  return coroutine.wrap(f, iterator.TAG)
end

function iterator.produce(...)
  return coroutine.yield(iterator.TAG, ...)
end

return iterator
