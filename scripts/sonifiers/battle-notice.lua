--[[
Battle Notice Sonifier - Audio alert while a force is under attack.

Per-force sonifier that checks periodically and plays a single alert while any player of the force has a combat alert
(entity_under_attack, entity_destroyed, turret_fire): the same alerts the game shows sighted players.

Uses Factorio's native player.play_sound() API, skipping vanilla mode players.
]]

local VanillaMode = require("scripts.vanilla-mode")

local mod = {}

-- Check interval in ticks
local CHECK_INTERVAL = 5 * 60

-- Combat-related alert types to check for
local COMBAT_ALERT_TYPES = {
   defines.alert_type.entity_under_attack,
   defines.alert_type.entity_destroyed,
   defines.alert_type.turret_fire,
}

---Check if a player has any combat-related alerts
---@param player LuaPlayer
---@return boolean
local function has_combat_alerts(player)
   for _, alert_type in ipairs(COMBAT_ALERT_TYPES) do
      local alerts = player.get_alerts({ type = alert_type })
      -- alerts is Dictionary[surface_index, Dictionary[alert_type, Alert[]]]
      for _, alerts_by_type in pairs(alerts) do
         if alerts_by_type[alert_type] and #alerts_by_type[alert_type] > 0 then return true end
      end
   end
   return false
end

---Where the newest combat alert on the player's surface is, as the game's alerts window lists it
---@param player LuaPlayer
---@return MapPosition?
function mod.newest_combat_alert_position(player)
   local newest
   for _, alert_type in ipairs(COMBAT_ALERT_TYPES) do
      local by_surface = player.get_alerts({ type = alert_type, surface = player.surface })
      local by_type = by_surface[player.surface.index]
      for _, alert in ipairs(by_type and by_type[alert_type] or {}) do
         if not newest or alert.tick > newest.tick then newest = alert end
      end
   end
   if not newest then return nil end
   -- A destroyed entity's alert keeps only its position
   if newest.target and newest.target.valid then return newest.target.position end
   return newest.position
end

---On tick handler - checks periodically for battle notifications
function mod.on_tick()
   local tick = game.tick
   if tick % CHECK_INTERVAL ~= 0 then return end

   for _, force in pairs(game.forces) do
      local under_attack = false
      for _, player in ipairs(force.players) do
         if player.connected and has_combat_alerts(player) then
            under_attack = true
            break
         end
      end

      if under_attack then
         for _, player in ipairs(force.players) do
            if player.connected and not VanillaMode.is_enabled(player.index) then
               player.play_sound({ path = "fa-battle-notice" })
            end
         end
      end
   end
end

return mod
