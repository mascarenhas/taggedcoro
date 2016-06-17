local coroutine = require "coroutine"
local ok, taggedcoro = pcall(require, "taggedcoro")
if ok then
  coroutine = taggedcoro.fortag("nlr")
end

local nlr = {}

function nlr.run(blk)
  local co = coroutine.wrap(blk)
  return co()
end

function nlr.ret(...)
  coroutine.yield(...)
end

return nlr
