local stm = require "taggedcoro.stm"
local thread = require "thread"

stm.var("balance", 100)

local function withdraw(amount)
  stm.transaction(function ()
    local balance = stm.get("balance")
    if balance < amount then
      stm.retry()
    end
    stm.set("balance", balance - amount)
  end)
end

local function balance()
  local bal
  stm.transaction(function ()
    bal = stm.get("balance")
  end)
  return bal
end

local function deposit(amount)
  stm.transaction(function ()
    local balance = stm.get("balance")
    stm.set("balance", balance + amount)
  end)
end

local t1 = thread.new(function ()
  stm.transaction(function ()
    print("t1:", balance())
    withdraw(200)
    print("t1:", balance())
  end)
end)

local t2 = thread.new(function ()
  stm.transaction(function ()
    print("t2:", balance())
    deposit(350)
    print("t2:",balance())
  end)
end)

thread.join()
