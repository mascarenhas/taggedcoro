
local coroutine = require "coroutine"
local ok, taggedcoro = pcall(require, "taggedcoro")
if ok then
  coroutine = taggedcoro.fortag("iterator")
end

local iterator = {}

function iterator.make(f)
  return coroutine.wrap(f)
end

function iterator.produce(...)
  return coroutine.yield(...)
end

return iterator
