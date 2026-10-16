--[[
What the scanner says of an entry it moved onto.

This runs only on the client of the player who scans, so it must only read: no storage, no change
to any entity or player, no math.random (its state is shared). Any of those would desync that client.
]]
local Localising = require("scripts.localising")
local Speech = require("scripts.speech")

local mod = {}

---What to say of an entry: the mod's own words, then an entity the DLL's tile reader reads as on its
---own tile (native/src/reader.h), so the scanner says of it what the cursor does, then more words.
---@class fa.scanner.Readout
---@field words LocalisedString?
---@field entity LuaEntity?
---@field after LocalisedString?
---@field sound SoundPath? played as the entry is said

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

---An alert on the map: its type as the alerts configuration names it, or a custom alert by its
---message; then the entity it is about, or what a destroyed one was.
---@param player LuaPlayer
---@param alert_type string as defines.alert_type names it
---@param entity LuaEntity? the entity it is about
---@param prototype string? what that entity is or was
---@param position MapPosition where the alert is
---@return fa.scanner.Readout
function mod.alert(player, alert_type, entity, prototype, position)
   ---@type LocalisedString
   local words = { "alerts-config-gui.name-" .. alert_type }
   if alert_type == "custom" then
      -- Only the player's alerts hold a custom alert's message
      local custom = defines.alert_type.custom
      local alerts = player.get_alerts({ surface = player.surface, type = custom })
      for _, alert in ipairs((alerts[player.surface_index] or {})[custom] or {}) do
         local target = alert.target
         local at = alert.position --[[@as MapPosition.struct?]]
         local point = position --[[@as MapPosition.struct]]
         if entity and target == entity or not entity and not target and at and at.x == point.x and at.y == point.y then
            words = assert(alert.message)
            break
         end
      end
   end
   if entity then return { words = words, entity = entity } end
   local message = Speech.MessageBuilder.new()
   message:list_item(words)
   if prototype then message:list_item(Localising.get_localised_name_with_fallback(prototypes.entity[prototype])) end
   return { words = message:build() }
end

---An entity, with a spawner's pollution.
---@param ent LuaEntity
---@return fa.scanner.Readout
function mod.of(ent)
   return { entity = ent, after = ent.type == "unit-spawner" and pollution(ent) or nil }
end

return mod
