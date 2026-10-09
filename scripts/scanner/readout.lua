--[[
What the scanner says of an entity it moved onto.

This runs only on the client of the player who scans, so it must only read: no storage, no change
to any entity or player, no math.random (its state is shared). Any of those would desync that client.
]]
local Info = require("scripts.fa-info")

local mod = {}

-- Spawners are grouped by how polluted they are. native/src/scanner.cpp kSpawnerPollution groups
-- them at these thresholds.
local SPAWNER_POLLUTION_BUCKETS = {
   { 0, { "fa.scanner-spawner-polluted-none" } },
   { 1, { "fa.scanner-spawner-polluted-lightly" } },
   { 99, { "fa.scanner-spawner-polluted-heavily" } },
}

---@param ent LuaEntity
---@return LocalisedString
local function pollution(ent)
   local level = 1
   for i = 1, #SPAWNER_POLLUTION_BUCKETS do
      if SPAWNER_POLLUTION_BUCKETS[i][1] <= ent.absorbed_pollution then
         level = i
      else
         break
      end
   end
   return SPAWNER_POLLUTION_BUCKETS[level][2]
end

-- fa-info's scanner readout, with a spawner's pollution.
---@param pindex integer
---@param ent LuaEntity
---@return LocalisedString
function mod.of(pindex, ent)
   local info = Info.ent_info(pindex, ent, true)
   if ent.type == "unit-spawner" then return { "fa.scanner-spawner-announce", info, pollution(ent) } end
   return info
end

return mod
