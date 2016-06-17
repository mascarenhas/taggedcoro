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

function M.create(f, tagset)
  tagset = tagset or { [DEFAULT_TAG] = true }
  local co = create(f)
  local meta = { tagset = tagset, stacked = false, lasttag = nil }
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

local function resumek(co, meta, ok, mark, tag, ...)
  for tag, _ in pairs(meta.tagset) do
    tagset[tag] = tagset[tag] - 1
  end
  if not ok then return false, mark end
  if status(co) == "dead" then
    meta.lasttag = "return"
    return ok, mark, tag, ...
  end
  if mark ~= MARK or not meta.tagset[tag] then
    meta.stacked = true
    return resumekk(co, meta, yield(mark, tag, ...))
  end
  meta.lasttag = tag
  return true, ...
end

function M.resume(co, ...)
  local meta = coros[co]
  if meta then
    if meta.stacked then return error("cannot resume stacked coroutine") end
    for tag, _ in pairs(meta.tagset) do
      tagset[tag] = (tagset[tag] or 0) + 1
    end
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

function M.lasttag(co)
  return coros[co].lasttag
end

function M.caller(co)
  return coros[co] and coros[co].caller
end

function M.tagset(co)
  return coros[co].tagset
end

function M.fortag(tag)
  return {
    running = M.running,
    create = function (f) return M.create(f, { [tag] = true }) end,
    yield = function (...)
      return M.yield(tag, ...)
    end,
    wrap = function (f) return M.wrap(f, {  [tag] = true }) end,
    isyieldable = function () return M.isyieldable(tag) end,
    status = M.status,
    resume = M.resume
  }
end

M.running = running

function M.isyieldable(tag)
  tag = tag or DEFAULT_TAG
  return isyieldable() and tagset[tag] and (tagset[tag] > 0)
end

local function wrapk(ok, err, ...)
  if ok then
    return err, ...
  else
    return error(err)
  end
end

function M.wrap(f, tagset)
  tagset = tagset or { [DEFAULT_TAG] = true }
  local co = M.create(f, tagset)
  return function (...)
           return wrapk(M.resume(co, ...))
         end, co
end

return M
