local coroutine = require("taggedcoro").fortag("stm")

local thread = require "thread"

local stm = {}

local db = {}

function stm.var(name, val)
  if coroutine.isyieldable("stm") then
    return error("cannot create stm variable " .. name .. " inside a transaction")
  end
  db[name] = { value = val, timestamp = 0, cv = thread.cv() }
end

function stm.transaction(blk)
  if coroutine.isyieldable() then
    return blk()
  end
  local co = coroutine.wrap(function ()
    blk()
    return "commit"
  end)
  local tvars = {}
  local request, name, val = co()
  while true do
    if request == "get" then
      if not tvars[name] then
        tvars[name] = { value = db[name].value, timestamp = db[name].timestamp, dirty = false }
      end
      request, name, val = co(tvars[name].value)
    elseif request == "set" then
      if not tvars[name] then
        tvars[name] = { value = val, timestamp = db[name].timestamp, dirty = true }
      else
        tvars[name].value = val
        tvars[name].dirty = true
      end
      request, name, val = co()
    elseif request == "retry" then
      for name, var in pairs(tvars) do
        if db[name].timestamp > var.timestamp then
          return stm.transaction(blk)
        end
      end
      local cvs = {}
      for name, _ in pairs(tvars) do
        cvs[#cvs+1] = db[name].cv
      end
      thread.yield("cvs", cvs)
      return stm.transaction(blk)
    elseif request == "commit" then
      for name, var in pairs(tvars) do
        if db[name].timestamp > var.timestamp then
          return stm.transaction(blk)
        end
      end
      for name, var in pairs(tvars) do
        if var.dirty then
          db[name].value = var.value
          db[name].timestamp = db[name].timestamp + 1
        end
      end
      for name, var in pairs(tvars) do
        if var.dirty then
          thread.signal(db[name].cv)
        end
      end
      break
    elseif request == "rollback" then
      break
    else
      return error("invalid stm operation " .. request)
    end
  end
end

function stm.get(name)
  return coroutine.yield("get", name)
end

function stm.set(name, val)
  return coroutine.yield("set", name, val)
end

function stm.retry()
  return coroutine.yield("retry")
end

function stm.rollback()
  return coroutine.yield("rollback")
end

return stm
