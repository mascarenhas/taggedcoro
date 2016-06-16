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
  if not M.isyieldable(tag) then return error("no coroutine for tag " .. tostring(tag)) end
  return yield(tag, ...)
end

local function resumekk(co, meta, ...)
  meta.stacked = false
  return M.resume(co, ...)
end

local function resumek(co, meta, ok, tag, ...)
  tagset[meta.tag] = tagset[meta.tag] - 1
  if not ok then return false, tag end
  if status(co) == "dead" then
    return ok, tag, ...
  end
  tag = tag or DEFAULT_TAG
  if meta.tag == tag then
    return true, ...
  else
    meta.stacked = true
    return resumekk(co, meta, yield(tag, ...))
  end
end

function M.resume(co, ...)
  local meta = coros[co]
  if meta.stacked then return error("cannot resume stacked coroutine") end
  tagset[meta.tag] = (tagset[meta.tag] or 0) + 1
  return resumek(co, meta, resume(co, ...))
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

local function wrapk(ok, err, ...)
  if ok then
    return err, ...
  else
    return error(err)
  end
end

function M.wrap(f, tag)
  tag = tag or DEFAULT_TAG
  local co = M.create(f, tag)
  return function (...)
           return wrapk(M.resume(co, ...))
         end
end

return M
