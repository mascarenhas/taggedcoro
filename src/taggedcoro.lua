local coroutine = require "coroutine"

local create = coroutine.create
local resume = coroutine.resume
local yield = coroutine.yield
local status = coroutine.status
local running = coroutine.running
local isyieldable = coroutine.isyieldable

local coros = setmetatable({}, { __mode = "k" })
local tagset = setmetatable({}, { __mode = "k" })

local M = {}

local DEFAULT_TAG = "coroutine"

function M.create(f, tag)
  tag = tag or DEFAULT_TAG
  local co = create(f)
  local meta = { tag = tag, stacked = false }
  coros[co] = meta
  return co
end

function M.yield(tag, ...)
  tag = tag or DEFAULT_TAG
  return yield(tag, ...)
end

function M.resume(co, ...)
  local meta = coros[co]
  if meta.stacked then return error("cannot resume stacked coroutine") end
  tagset[meta.tag] = (tagset[meta.tag] or 0) + 1
  local res = { resume(co, ...) }
  tagset[meta.tag] = tagset[meta.tag] - 1
  if not res[1] then return false, res[2] end
  if status(co) == "dead" then
    return table.unpack(res)
  end
  local tag = res[2] or DEFAULT_TAG
  if meta.tag == tag then
    return true, table.unpack(res, 3)
  else
    if not isyieldable() then return error("no coroutine for tag " .. tostring(tag)) end
    meta.stacked = true
    local res = { yield(tag, table.unpack(res, 3)) }
    meta.stacked = false
    return M.resume(co, table.unpack(res))
  end
end

function M.status(co)
  if coros[co].stacked then
    return "stacked"
  else
    return status(co)
  end
end

M.running = running

function M.isyieldable(tag)
  tag = tag or DEFAULT_TAG
  return tagset[tag] and (tagset[tag] > 0)
end

function M.wrap(f, tag)
  tag = tag or DEFAULT_TAG
  local co = M.create(f, tag)
  return function (...)
           local res = { M.resume(co, ...) }
           if res[1] then
             return table.unpack(res, 2)
           else
             return error(res[2])
           end
         end
end

return M
