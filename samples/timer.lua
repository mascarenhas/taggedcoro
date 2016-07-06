
local thread = require "thread"
local iterator = require "taggedcoro.iterator"
local cosmo = require "cosmo"
local nlr = require "taggedcoro.nlr"
local ex = require "taggedcoro.exception"

local template = cosmo.compile("$message[[$msg]]")

local function aux(msg)
  return nlr.run(function ()
    local msg = ex.throw(msg)
    nlr.ret(msg)
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
  ex.trycatch(function ()
    for msg in iter_waiter(time, msg) do
      print(msg)
    end
  end,
  function (retry, traceback, err)
    --print(traceback("error in waiter"))
    retry(err:upper())
  end)
end

thread.new(waiter, 1000, "hi")
thread.new(waiter, 4000, "hello")

thread.join()
