
local thread = require "thread"
local iterator = require "iterator"
local cosmo = require "cosmo"
local nlr = require "nlr"
local ex = require "exception"

local template = cosmo.compile("$message[[$msg]]")

local function aux(msg)
  return nlr.run(function ()
    ex.trycatch(function ()
      local msg = ex.throw(msg)
      nlr.ret(msg)
    end,
    function (err, retry)
      retry(err)
    end)
  end)
end

local function iter_waiter(time, msg)
  return iterator.make(function ()
    while true do
      local msg = template {
        message = function ()
          thread.sleep(time)
          iterator.produce("The message is:")
          cosmo.yield({ msg = msg })
        end
      }
      iterator.produce(aux(msg))
    end
  end)
end

local function waiter(time, msg)
  for msg in iter_waiter(time, msg) do
    print(msg)
  end
end

thread.new(waiter, 1000, "hi")
thread.new(waiter, 4000, "hello")

thread.join()
