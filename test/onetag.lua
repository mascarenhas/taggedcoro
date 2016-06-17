local taggedcoro = require("taggedcoro").fortag("coroutine")

local debug = require "debug"

local f

local main, ismain = coroutine.running()
assert(ismain)
assert(not coroutine.resume(main))
assert(not coroutine.isyieldable())
assert(not pcall(coroutine.yield))


-- tests for multiple yield/resume arguments

local function eqtab (t1, t2)
  assert(#t1 == #t2)
  for i = 1, #t1 do
    local v = t1[i]
    assert(t2[i] == v)
  end
end

_G.x = nil   -- declare x
function foo (a, ...)
  local x, y = coroutine.running()
  assert(x == f and y == false)
  -- next call should not corrupt coroutine (but must fail,
  -- as it attempts to resume the running coroutine)
  assert(coroutine.resume(f) == false)
  assert(coroutine.status(f) == "running")
  local arg = {...}
  assert(coroutine.isyieldable())
  for i=1,#arg do
    _G.x = {coroutine.yield(table.unpack(arg[i]))}
  end
  return table.unpack(a)
end

f = coroutine.create(foo)
assert(coroutine.status(f) == "suspended")
local s,a,b,c,d
s,a,b,c,d = coroutine.resume(f, {1,2,3}, {}, {1}, {'a', 'b', 'c'})
assert(s)
assert(a == nil)
assert(coroutine.status(f) == "suspended")
s,a,b,c,d = coroutine.resume(f)
eqtab(_G.x, {})
assert(s and a == 1 and b == nil)
s,a,b,c,d = coroutine.resume(f, 1, 2, 3)
eqtab(_G.x, {1, 2, 3})
assert(s and a == 'a' and b == 'b' and c == 'c' and d == nil)
s,a,b,c,d = coroutine.resume(f, "xuxu")
eqtab(_G.x, {"xuxu"})
assert(s and a == 1 and b == 2 and c == 3 and d == nil)
assert(coroutine.status(f) == "dead")
s, a = coroutine.resume(f, "xuxu")
assert(not s and string.find(a, "dead") and coroutine.status(f) == "dead")

-- yields in tail calls
local function foo (i) return coroutine.yield(i) end
f = coroutine.wrap(function ()
  for i=1,10 do
    assert(foo(i) == _G.x)
  end
  return 'a'
end)
for i=1,10 do _G.x = i; assert(f(i) == i) end
_G.x = 'xuxu'; assert(f('xuxu') == 'a')

-- recursive
function pf (n, i)
  coroutine.yield(n)
  pf(n*i, i+1)
end

f = coroutine.wrap(pf)
local s=1
for i=1,10 do
  assert(f(1, 1) == s)
  s = s*i
end

-- sieve
function gen (n)
  return coroutine.wrap(function ()
    for i=2,n do coroutine.yield(i) end
  end)
end


function filter (p, g)
  return coroutine.wrap(function ()
    while 1 do
      local n = g()
      if n == nil then return end
      if math.fmod(n, p) ~= 0 then coroutine.yield(n) end
    end
  end)
end

local x = gen(100)
local a = {}
while 1 do
  local n = x()
  if n == nil then break end
  table.insert(a, n)
  x = filter(n, x)
end

assert(#a == 25 and a[#a] == 97)
x, a = nil

-- yielding across C boundaries

co = coroutine.wrap(function()
       assert(not pcall(table.sort,{1,2,3}, coroutine.yield))
       assert(coroutine.isyieldable())
       coroutine.yield(20)
       return 30
     end)

assert(co() == 20)
assert(co() == 30)

local f = function (s, i) return coroutine.yield(i) end

local f1 = coroutine.wrap(function ()
             return xpcall(pcall, function (...) return ... end,
               function ()
                 local s = 0
                 for i in f, nil, 1 do pcall(function () s = s + i end) end
                 error({s})
               end)
           end)

f1()
for i = 1, 10 do assert(f1(i) == i) end
local r1, r2, v = f1(nil)
assert(r1 and not r2 and v[1] ==  (10 + 1)*10/2)


function f (a, b) a = coroutine.yield(a);  error{a + b} end
function g(x) return x[1]*2 end

co = coroutine.wrap(function ()
       coroutine.yield(xpcall(f, g, 10, 20))
     end)

assert(co() == 10)
r, msg = co(100)
assert(not r and msg == 240)

-- unyieldable C call
do
  local function f (c)
          return c .. c
        end

  local co = coroutine.wrap(function (c)
               assert(coroutine.isyieldable())
               local s = string.gsub("a", ".", f)
               return s
             end)
  assert(co() == "aa")
end

-- errors in coroutines
function foo ()
  assert(debug.getinfo(1).currentline == debug.getinfo(foo).linedefined + 1)
  assert(debug.getinfo(2).currentline == debug.getinfo(goo).linedefined)
  coroutine.yield(3)
  error(foo)
end

function goo() foo() end
x = coroutine.wrap(goo)
assert(x() == 3)
local a,b = pcall(x)
assert(not a and b == foo)

x = coroutine.create(goo)
a,b = coroutine.resume(x)
assert(a and b == 3)
a,b = coroutine.resume(x)
assert(not a and b == foo and coroutine.status(x) == "dead")
a,b = coroutine.resume(x)
assert(not a and string.find(b, "dead") and coroutine.status(x) == "dead")

-- co-routines x for loop
function all (a, n, k)
  if k == 0 then coroutine.yield(a)
  else
    for i=1,n do
      a[k] = i
      all(a, n, k-1)
    end
  end
end

local a = 0
for t in coroutine.wrap(function () all({}, 5, 4) end) do
  a = a+1
end
assert(a == 5^4)

-- access to locals of collected corroutines
local C = {}; setmetatable(C, {__mode = "kv"})
local x = coroutine.wrap (function ()
            local a = 10
            local function f () a = a+10; return a end
            while true do
              a = a+1
              coroutine.yield(f)
            end
          end)

C[1] = x;

local f = x()
assert(f() == 21 and x()() == 32 and x() == f)
x = nil
collectgarbage()
assert(C[1] == nil)
assert(f() == 43 and f() == 53)

-- old bug: attempt to resume itself

function co_func (current_co)
  assert(coroutine.running() == current_co)
  assert(coroutine.resume(current_co) == false)
  coroutine.yield(10, 20)
  assert(coroutine.resume(current_co) == false)
  coroutine.yield(23)
  return 10
end

local co = coroutine.create(co_func)
local a,b,c = coroutine.resume(co, co)
assert(a == true and b == 10 and c == 20)
a,b = coroutine.resume(co, co)
assert(a == true and b == 23)
a,b = coroutine.resume(co, co)
assert(a == true and b == 10)
assert(coroutine.resume(co, co) == false)
assert(coroutine.resume(co, co) == false)

-- attempt to resume 'normal' coroutine
local co1, co2
co1 = coroutine.create(function () return co2() end)
co2 = coroutine.wrap(function ()
        assert(coroutine.status(co1) == 'normal')
        assert(not coroutine.resume(co1))
        coroutine.yield(3)
      end)

a,b = coroutine.resume(co1)
assert(a and b == 3)
assert(coroutine.status(co1) == 'dead')

-- infinite recursion of coroutines
a = function(a) coroutine.wrap(a)(a) end
assert(not pcall(a, a))
a = nil

-- access to locals of erroneous coroutines
local x = coroutine.create (function ()
            local a = 10
            _G.f = function () a=a+1; return a end
            error('x')
          end)

assert(not coroutine.resume(x))
-- overwrite previous position of local `a'
assert(not coroutine.resume(x, 1, 1, 1, 1, 1, 1, 1))
assert(_G.f() == 11)
assert(_G.f() == 12)

-- leaving a pending coroutine open
_X = coroutine.wrap(function ()
      local a = 10
      local x = function () a = a+1 end
      coroutine.yield()
    end)

_X()


if not _soft then
  -- bug (stack overflow)
  local j = 2^9
  local lim = 1000000    -- (C stack limit; assume 32-bit machine)
  local t = {lim - 10, lim - 5, lim - 1, lim, lim + 1}
  for i = 1, #t do
    local j = t[i]
    co = coroutine.create(function()
           local t = {}
           for i = 1, j do t[i] = i end
           return table.unpack(t)
         end)
    local r, msg = coroutine.resume(co)
    assert(not r)
  end
  co = nil
end

assert(coroutine.running() == main)

print"+"

print"testing yields inside metamethods"

local mt = {
  __eq = function(a,b) coroutine.yield(nil, "eq"); return a.x == b.x end,
  __lt = function(a,b) coroutine.yield(nil, "lt"); return a.x < b.x end,
  __le = function(a,b) coroutine.yield(nil, "le"); return a - b <= 0 end,
  __add = function(a,b) coroutine.yield(nil, "add"); return a.x + b.x end,
  __sub = function(a,b) coroutine.yield(nil, "sub"); return a.x - b.x end,
  __mod = function(a,b) coroutine.yield(nil, "mod"); return a.x % b.x end,
  __unm = function(a,b) coroutine.yield(nil, "unm"); return -a.x end,
  __bnot = function(a,b) coroutine.yield(nil, "bnot"); return ~a.x end,
  __shl = function(a,b) coroutine.yield(nil, "shl"); return a.x << b.x end,
  __shr = function(a,b) coroutine.yield(nil, "shr"); return a.x >> b.x end,
  __band = function(a,b)
             a = type(a) == "table" and a.x or a
             b = type(b) == "table" and b.x or b
             coroutine.yield(nil, "band")
             return a & b
           end,
  __bor = function(a,b) coroutine.yield(nil, "bor"); return a.x | b.x end,
  __bxor = function(a,b) coroutine.yield(nil, "bxor"); return a.x ~ b.x end,

  __concat = function(a,b)
               coroutine.yield(nil, "concat");
               a = type(a) == "table" and a.x or a
               b = type(b) == "table" and b.x or b
               return a .. b
             end,
  __index = function (t,k) coroutine.yield(nil, "idx"); return t.k[k] end,
  __newindex = function (t,k,v) coroutine.yield(nil, "nidx"); t.k[k] = v end,
}


local function new (x)
  return setmetatable({x = x, k = {}}, mt)
end


local a = new(10)
local b = new(12)
local c = new"hello"

local function run (f, t)
  local i = 1
  local c = coroutine.wrap(f)
  while true do
    local res, stat = c()
    if res then assert(t[i] == nil); return res, t end
    assert(stat == t[i])
    i = i + 1
  end
end


assert(run(function () if (a>=b) then return '>=' else return '<' end end,
       {"le", "sub"}) == "<")
-- '<=' using '<'
mt.__le = nil
assert(run(function () if (a<=b) then return '<=' else return '>' end end,
       {"lt"}) == "<=")
assert(run(function () if (a==b) then return '==' else return '~=' end end,
       {"eq"}) == "~=")

assert(run(function () return a & b + a end, {"add", "band"}) == 2)

assert(run(function () return a % b end, {"mod"}) == 10)

assert(run(function () return ~a & b end, {"bnot", "band"}) == ~10 & 12)
assert(run(function () return a | b end, {"bor"}) == 10 | 12)
assert(run(function () return a ~ b end, {"bxor"}) == 10 ~ 12)
assert(run(function () return a << b end, {"shl"}) == 10 << 12)
assert(run(function () return a >> b end, {"shr"}) == 10 >> 12)

assert(run(function () return a..b end, {"concat"}) == "1012")

assert(run(function() return a .. b .. c .. a end,
       {"concat", "concat", "concat"}) == "1012hello10")

assert(run(function() return "a" .. "b" .. a .. "c" .. c .. b .. "x" end,
       {"concat", "concat", "concat"}) == "ab10chello12x")


do   -- a few more tests for comparsion operators
  local mt1 = {
    __le = function (a,b)
      coroutine.yield(10)
      return
        (type(a) == "table" and a.x or a) <= (type(b) == "table" and b.x or b)
    end,
    __lt = function (a,b)
      coroutine.yield(10)
      return
        (type(a) == "table" and a.x or a) < (type(b) == "table" and b.x or b)
    end,
  }
  local mt2 = { __lt = mt1.__lt }   -- no __le

  local function run (f)
    local co = coroutine.wrap(f)
    local res
    repeat
      res = co()
    until res ~= 10
    return res
  end

  local function test ()
    local a1 = setmetatable({x=1}, mt1)
    local a2 = setmetatable({x=2}, mt2)
    assert(a1 < a2)
    assert(a1 <= a2)
    assert(1 < a2)
    assert(1 <= a2)
    assert(2 > a1)
    assert(2 >= a2)
    return true
  end

  run(test)

end

assert(run(function ()
             a.BB = print
             return a.BB
           end, {"nidx", "idx"}) == print)

-- getuptable & setuptable
do local _ENV = _ENV
  f = function () AAA = BBB + 1; return AAA end
end
g = new(10); g.k.BBB = 10;
debug.setupvalue(f, 1, g)
assert(run(f, {"idx", "nidx", "idx"}) == 11)
assert(g.k.AAA == 11)

print"+"

print"testing yields inside 'for' iterators"

local f = function (s, i)
      if i%2 == 0 then coroutine.yield(nil, "for") end
      if i < s then return i + 1 end
    end

assert(run(function ()
             local s = 0
             for i in f, 4, 0 do s = s + i end
             return s
           end, {"for", "for", "for"}) == 10)

-- bug in nCcalls
local co = coroutine.wrap(function ()
local a = {pcall(pcall,pcall,pcall,pcall,pcall,pcall,pcall,error,"hi")}
return pcall(assert, table.unpack(a))
end)

local a = {co()}
assert(a[10] == "hi")
print'OK'
