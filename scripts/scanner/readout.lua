--[[
What the scanner says of an entry it moved onto.

This runs only on the client of the player who scans, so it must only read: no storage, no change
to any entity or player, no math.random (its state is shared). Any of those would desync that client.
]]
local mod = {}

---What to say of an entry: the mod's own words, then an entity the DLL's tile reader reads as on its
---own tile (native/src/reader.h), so the scanner says of it what the cursor does, then more words.
---@class fa.scanner.Readout
---@field words LocalisedString?
---@field entity LuaEntity?
---@field after LocalisedString?

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

---An entity, with a spawner's pollution.
---@param ent LuaEntity
---@return fa.scanner.Readout
function mod.of(ent)
   return { entity = ent, after = ent.type == "unit-spawner" and pollution(ent) or nil }
end

return mod
