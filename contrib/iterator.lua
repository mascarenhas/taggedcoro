local coroutine = require("taggedcoro").fortag("iterator")

local iterator = {}

function iterator.make(f)
  return coroutine.wrap(f)
end

function iterator.produce(...)
  return coroutine.yield(...)
end

return iterator
