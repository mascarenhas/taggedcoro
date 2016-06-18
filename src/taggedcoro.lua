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

local MARK = {}

local DEFAULT_TAG = "coroutine"

function M.create(tag, f)
  tag = tag or DEFAULT_TAG
  local co = create(f)
  local meta = { tag = tag, stacked = false }
  coros[co] = meta
  return co
end

function M.yield(tag, ...)
  tag = tag or DEFAULT_TAG
  if not M.isyieldable(tag) then return error("no coroutine for tag " .. tostring(tag)) end
  return yield(MARK, tag, ...)
end

local function resumekk(co, meta, ...)
  meta.stacked = false
  return M.resume(co, ...)
end

local function resumek(co, meta, ok, ...)
  tagset[meta.tag] = tagset[meta.tag] - 1
  if not ok then return false, ... end
  if status(co) == "dead" then
    return ok, ...
  end
  local mark, tag = ...
  if mark ~= MARK or meta.tag ~= tag then
    meta.stacked = true
    return resumekk(co, meta, yield(...))
  end
  return ok, select(3, ...)
end

function M.resume(co, ...)
  local meta = coros[co]
  if meta then
    if meta.stacked then return error("cannot resume stacked coroutine") end
    tagset[meta.tag] = (tagset[meta.tag] or 0) + 1
    meta.caller = M.running()
    return resumek(co, meta, resume(co, ...))
  else
    return resume(co, ...)
  end
end

function M.status(co)
  if coros[co].stacked then
    return "stacked"
  else
    return status(co)
  end
end

function M.caller(co)
  return coros[co] and coros[co].caller
end

function M.tag(co)
  return coros[co].tag
end

M.running = running

function M.isyieldable(tag)
  tag = tag or DEFAULT_TAG
  return isyieldable() and tagset[tag] and (tagset[tag] > 0)
end

function M.fortag(tag)
  return {
    running = M.running,
    create = function (f) return M.create(tag, f) end,
    yield = function (...)
      return M.yield(tag, ...)
    end,
    wrap = function (f) return M.wrap(tag, f) end,
    isyieldable = function () return M.isyieldable(tag) end,
    status = M.status,
    resume = M.resume
  }
end

local function wrapk(ok, ...)
  if ok then
    return ...
  else
    return error((...))
  end
end

function M.wrap(tag, f)
  tag = tag or DEFAULT_TAG
  return debug.setmetatable(M.create(tag, f),
                            { __call = function (co, ...)
                                         return wrapk(M.resume(co, ...))
                                       end })
end

return M
