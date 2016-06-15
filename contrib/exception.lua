local coroutine = require "taggedcoro"

local ex = { TAG = "exception" }

function ex.trycatch(tblk, cblk)
  local co = coroutine.wrap(function ()
    return true, tblk()
  end, ex.TAG)
  local res = { co() }
  if res[1] then
    return table.unpack(res, 2)
  else
    return cblk(res[2], co)
  end
end

function ex.throw(e)
  return coroutine.yield(ex.TAG, false, e)
end

return ex
