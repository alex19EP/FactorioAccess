--Here: what a player's deconstruction or upgrade marks did, said once all of them are in.
--
-- A finished selection marks or unmarks every entity in it within one tick, and the game raises an
-- event for each. They are counted per player and kind, and the totals are said on the next tick.
-- Building a blueprint over entities marks them too, and is said the same way. Speech only: the
-- counts never reach storage.

local Speech = require("scripts.speech")

local mod = {}

---@alias fa.SelectionResults.Kind "deconstruct"|"cancel-deconstruct"|"upgrade"|"cancel-upgrade"

-- The order the kinds are said in.
---@type fa.SelectionResults.Kind[]
local KINDS = { "deconstruct", "cancel-deconstruct", "upgrade", "cancel-upgrade" }

---@type table<integer, table<fa.SelectionResults.Kind, integer>>
local pending = {}

---@param kind fa.SelectionResults.Kind
---@return fun(event: { player_index: integer? })
local function counter(kind)
   return function(event)
      local pindex = event.player_index
      if not pindex then return end
      local counts = pending[pindex] or {}
      pending[pindex] = counts
      counts[kind] = (counts[kind] or 0) + 1
   end
end

mod.on_marked_for_deconstruction = counter("deconstruct")
mod.on_cancelled_deconstruction = counter("cancel-deconstruct")
mod.on_marked_for_upgrade = counter("upgrade")
mod.on_cancelled_upgrade = counter("cancel-upgrade")

function mod.on_tick()
   if not next(pending) then return end
   for pindex, counts in pairs(pending) do
      local message = Speech.MessageBuilder.new()
      for _, kind in ipairs(KINDS) do
         if counts[kind] then message:list_item({ "fa.selection-result-" .. kind, counts[kind] }) end
      end
      Speech.speak(pindex, message:build())
   end
   pending = {}
end

return mod
