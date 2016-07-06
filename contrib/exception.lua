local coroutine = require("taggedcoro").fortag("exception")

local ex = {}

local function trycatchk(cblk, co, ok, ...)
  if ok and coroutine.status(co) == "dead" then
      return ...
  else
    local resume
    if ok then
      resume = function (v)
        return trycatchk(cblk, co, coroutine.resume(co, v))
      end
    end
    local traceback = function (msg)
      return coroutine.traceback(co, msg)
    end
    return cblk(resume, traceback, ...)
  end
end

function ex.trycatch(tblk, cblk)
  local co = coroutine.create(tblk)
  return trycatchk(cblk, co, coroutine.resume(co))
end

function ex.throw(...)
  return coroutine.yield(...)
end

return ex
