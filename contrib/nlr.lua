local coroutine = require "taggedcoro"

local nlr = { TAG = "return" }

function nlr.run(blk)
  local co = coroutine.wrap(blk, nlr.TAG)
  return co()
end

function nlr.ret(...)
  coroutine.yield(nlr.TAG, ...)
end

return nlr
