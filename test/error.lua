local coroutine = require "taggedcoro"

local function zero()
  coroutine.yield("foo", "zero")
end

local function one()
  print("one", coroutine.running())
  zero()
end

local cone = coroutine.wrap("one", one)

local function two()
  print("two", coroutine.running())
  cone()
end

local cotwo = coroutine.create("two", two)
local ctwo = coroutine.wrap("two", cotwo)
--two()
print("main", coroutine.running())


--local ok, err = coroutine.resume(cotwo)
--print(coroutine.traceback(cotwo, err))

local ok, err = pcall(coroutine.call, cotwo)
print(ok, err)
print(coroutine.traceback(cotwo, err))

--xpcall(ctwo, function(msg)
--  print(coroutine.traceback(msg))
--  return msg
--end)
