local yield = coroutine.yield
local wrap = coroutine.wrap
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
local ctwo = coroutine.wrap(cotwo)
--local ctwo = wrap(two)
--two()
print("main", coroutine.running())


--local ok, err = coroutine.resume(cotwo)
--print(coroutine.traceback(cotwo, err))

print(xpcall(ctwo, function(msg)
  print(coroutine.traceback(msg, 1))
  return msg
end))
