local coroutine = require("taggedcoro").fortag("nlr")

local nlr = {}

function nlr.run(blk)
  local co = coroutine.wrap(blk)
  return co()
end

function nlr.ret(...)
  coroutine.yield(...)
end

return nlr
