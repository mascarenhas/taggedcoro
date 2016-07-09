local coroutine = coroutine
local tc = require "taggedcoro"

local _assert = assert

local function assert(x)
  _assert(x)
  io.write(".")
end

local usetc = true
local tag = "notfound"

local function zero()
  if usetc then
    assert(tc.isyieldable("one"))
    assert(not tc.isyieldable("notfound"))
    tc.yield(tag, "zero")
  else
    assert(tc.isyieldable("one"))
    assert(not tc.isyieldable("notfound"))
    local ok, err = coroutine.yield("zero")
    assert(not ok)
    assert(err:match("untagged coroutine not found"))
    error(err)
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
  local cotwo = tc.create("two", two)
  local ctwo = tc.wrap(cotwo)
  local ok, err = tc.resume(cotwo)
  assert(not ok)
  assert(tc.tag(cotwo) == "two")
  assert(tc.parent(cotwo) == tc.running())
  assert(tc.parent(tc.source(cotwo)) == cotwo)
  assert(tc.source(tc.running()) == tc.source(cotwo))
  assert(tc.tag(tc.source(cotwo)) == "one")
  assert(err:match("tag notfound not found"))
  assert(tc.traceback() == tc.traceback(cotwo))
  assert(tc.traceback():match("^[^\n]*\n[^\n]*\n[^\n]*\n[^\n]*\n[^\n]*\n[^\n]*"))
end

do
  usetc = true
  local oldzero = zero
  zero = tc.wrap("zero", zero)
  local ctwo, cotwo = tc.wrap("two", two)
  local ok, err = tc.resume(cotwo)
  assert(not ok)
  assert(tc.tag(cotwo) == "two")
  assert(tc.parent(cotwo) == tc.running())
  assert(tc.parent(tc.parent(tc.source(cotwo))) == cotwo)
  assert(tc.source(tc.running()) == tc.source(cotwo))
  assert(tc.tag(tc.source(cotwo)) == "zero")
  assert(err:match("tag notfound not found"))
  assert(tc.traceback() == tc.traceback(cotwo))
  assert(tc.traceback():match("^[^\n]*\n[^\n]*\n[^\n]*\n[^\n]*\n[^\n]*\n[^\n]*"))
  zero = oldzero
end

do
  usetc = true
  tag = "two"
  local ctwo, cotwo = tc.wrap("two", two)
  local ok, err = tc.resume(cotwo)
  assert(ok)
  assert(tc.tag(cotwo) == "two")
  assert(tc.parent(cotwo) == tc.running())
  assert(tc.parent(tc.source(cotwo)) == cotwo)
  assert(tc.source(tc.running()) ~= tc.source(cotwo))
  assert(tc.tag(tc.source(cotwo)) == "one")
  assert(tc.status(tc.source(cotwo)) == "stacked")
  assert(err:match("zero"))
  local ok, err = tc.resume(tc.source(cotwo))
  assert(not ok)
  assert(err:match("stacked"))
  tag = "notfound"
end

do
  usetc = false
  local ctwo, cotwo = tc.wrap("two", two)
  local ok, err = tc.resume(cotwo)
  assert(not ok)
  assert(tc.tag(cotwo) == "two")
  assert(tc.parent(cotwo) == tc.running())
  assert(tc.parent(tc.source(cotwo)) == cotwo)
  assert(tc.source(tc.running()) == tc.source(cotwo))
  assert(err:match("untagged coroutine not found"))
  assert(tc.traceback() == tc.traceback(cotwo))
end

do
  usetc = false
  local oldzero = zero
  zero = tc.wrap("zero", zero)
  local ctwo, cotwo = tc.wrap("two", two)
  local ok, err = tc.resume(cotwo)
  assert(not ok)
  assert(tc.tag(cotwo) == "two")
  assert(tc.parent(cotwo) == tc.running())
  assert(tc.parent(tc.parent(tc.source(cotwo))) == cotwo)
  assert(tc.source(tc.running()) == tc.source(cotwo))
  assert(err:match("untagged coroutine not found"))
  assert(tc.traceback() == tc.traceback(cotwo))
  zero = oldzero
end

do
  local cotwo = coroutine.create(two)
  local ok, err = tc.resume(cotwo)
  assert(not ok)
  assert(not tc.source(cotwo))
  assert(err:match("cannot resume untagged"))
end

do
  usetc = true
  local oldzero, cozero = zero
  zero, cozero = tc.wrap("zero", zero)
  local cotwo = coroutine.create(two)
  local ok, err = coroutine.resume(cotwo)
  assert(not ok)
  assert(not tc.source(cotwo))
  assert(err:match("attempt to yield across untagged"))
  assert(tc.traceback(cozero):match("reached untagged coroutine"))
  zero = oldzero
end

do
  usetc = true
  local ctwo, cotwo = tc.wrap("two", two)
  local ok, tb = xpcall(ctwo, function (msg)
    return tc.traceback(msg, 1)
  end)
  assert(not ok)
  assert(tc.tag(cotwo) == "two")
  assert(tc.parent(cotwo) == tc.running())
  assert(tc.parent(tc.source(cotwo)) == cotwo)
  assert(tc.source(tc.running()) == tc.source(cotwo))
  assert(tc.tag(tc.source(cotwo)) == "one")
  assert(tb:match("tag notfound not found"))
  assert(tc.traceback() ~= tb)
end

do
  local tctwo = tc.fortag("two")
  usetc = true
  local cotwo = tctwo.create(two)
  local ctwo = tctwo.wrap(cotwo)
  local ok, tb = xpcall(ctwo, function (msg)
    return tc.traceback(msg, 1)
  end)
  assert(not ok)
  assert(tc.tag(cotwo) == "two")
  assert(tc.parent(cotwo) == tc.running())
  assert(tc.parent(tc.source(cotwo)) == cotwo)
  assert(tc.source(tc.running()) == tc.source(cotwo))
  assert(tc.tag(tc.source(cotwo)) == "one")
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
  assert(tc.tag(cotwo) == "two")
  assert(tc.parent(cotwo) == tc.running())
  assert(tc.parent(tc.source(cotwo)) == cotwo)
  assert(tc.source(tc.running()) == tc.source(cotwo))
  assert(tb:match("untagged coroutine not found"))
  assert(tc.traceback() ~= tb)
end

do
  usetc = true
  local oldzero, cozero = zero
  zero, cozero = tc.wrap("zero", zero)
  local ctwo = coroutine.wrap(two)
  local ok, tb = xpcall(ctwo, function (msg)
    return tc.traceback(msg, 1)
  end)
  assert(not ok)
  assert(tb:match("attempt to yield across untagged"))
  assert(tc.traceback(cozero):match("reached untagged coroutine"))
  zero = oldzero
end

do
  usetc = false
  local oldzero, cozero = zero
  zero, cozero = tc.wrap("zero", zero)
  local ctwo = coroutine.wrap(two)
  local ok, tb = xpcall(ctwo, function (msg)
    return tc.traceback(msg, 1)
  end)
  assert(ok)
  assert(tb == "zero")
  zero = oldzero
end

do
  local oldzero = zero
  zero = function ()
    tc.yield("two", "zero")
  end
  local cone = tc.wrap("one", one)
  local ctwo = tc.wrap("two", function ()
    return xpcall(function () assert(false) end, function (msg)
      local ok, err = pcall(cone)
      assert(not ok)
      return err
    end)
  end)
  local _, err = ctwo()
  assert(err:match("attempt to yield"))
  zero = oldzero
end

if _VERSION ~= "Lua 5.1" then
  tc = tc.install()

  do
    usetc = true
    local ctwo, cotwo = tc.wrap("two", two)
    local ok, tb = xpcall(ctwo, function (msg)
      return tc.traceback(msg, 1)
    end)
    assert(not ok)
    assert(cotwo:tag() == "two")
    assert(cotwo:parent() == tc.running())
    assert(cotwo:source():parent() == cotwo)
    assert(tc.running():source() == cotwo:source())
    assert(cotwo:source():tag() == "one")
    assert(tb:match("tag notfound not found"))
    assert(tc.traceback() ~= tb)
  end
end

print()
