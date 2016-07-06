local coroutine = coroutine
local tc = require "taggedcoro"

local _assert = assert

local function assert(x)
  _assert(x)
  io.write(".")
end

local usetc = true

local function zero()
  if usetc then
    tc.yield("notfound", "zero")
  else
    coroutine.yield("zero")
  end
end

local function one()
  zero()
end

local function two()
  local cone = tc.wrap("one", one)
  cone()
end

do
  usetc = true
  local ctwo, cotwo = tc.wrap("two", two)
  local ok, err = tc.resume(cotwo)
  assert(not ok)
  assert(err:match("tag notfound not found"))
  assert(tc.traceback() == tc.traceback(cotwo))
end

do
  usetc = false
  local ctwo, cotwo = tc.wrap("two", two)
  local ok, err = tc.resume(cotwo)
  assert(not ok)
  assert(err:match("attempt to yield"))
  assert(tc.traceback() == tc.traceback(cotwo))
end

do
  usetc = true
  local cotwo = coroutine.create(two)
  local ok, err = tc.resume(cotwo)
  assert(not ok)
  assert(err:match("untagged"))
end

do
  usetc = true
  local ctwo, cotwo = tc.wrap("two", two)
  local ok, tb = xpcall(ctwo, function (msg)
    return tc.traceback(msg, 1)
  end)
  assert(not ok)
  assert(tb:match("tag notfound not found"))
  assert(tc.traceback() ~= tb)
end

do
  usetc = false
  local ctwo, cotwo = tc.wrap("two", two)
  local ok, tb = xpcall(ctwo, function (msg)
    return tc.traceback(msg, 1)
  end)
  assert(not ok)
  assert(tb:match("attempt to yield"))
  assert(tc.traceback() ~= tb)
end

do
  usetc = true
  local ctwo = coroutine.wrap(two)
  local ok, tb = xpcall(ctwo, function (msg)
    return tc.traceback(msg, 1)
  end)
  assert(not ok)
  assert(tb:match("untagged"))
end

print()
