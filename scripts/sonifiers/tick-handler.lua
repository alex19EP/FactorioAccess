--[[
Centralized tick handler for all sonifiers.

This module runs the player loop once and calls each sonifier's
on_tick_per_player function, avoiding redundant iteration.
]]

local EventManager = require("scripts.event-manager")
local VanillaMode = require("scripts.vanilla-mode")
local SettingsDecls = require("scripts.settings-decls")
local SETTING_NAMES = SettingsDecls.SETTING_NAMES
local VehicleSounds = require("scripts.sonifiers.vehicle")
local InserterSonifier = require("scripts.sonifiers.inserter")
local PlayerCraftingSonifier = require("scripts.sonifiers.player-crafting")
local GridSonifier = require("scripts.sonifiers.grid-sonifier")
local EnemyRadar = require("scripts.sonifiers.combat.enemy-radar")
local SpawnerRadar = require("scripts.sonifiers.combat.spawner-radar")
local HealthBar = require("scripts.sonifiers.health-bar")

local mod = {}

---Called every tick from control.lua
function mod.on_tick()
   for _, player in pairs(game.players) do
      if not player.connected then goto continue end

      local pindex = player.index
      if VanillaMode.is_enabled(pindex) then goto continue end

      -- Vehicle sounds (driving feedback)
      VehicleSounds.on_tick_per_player(pindex)

      -- Inserter sonification (when hovering inserter)
      InserterSonifier.on_tick_per_player(pindex)

      local mod_settings = player.mod_settings

      -- Player crafting sounds
      if mod_settings[SETTING_NAMES.HANDCRAFTING_SOUND].value then PlayerCraftingSonifier.on_tick_per_player(pindex) end

      -- Grid-based sonification (crafting machines etc)
      if mod_settings[SETTING_NAMES.SONIFICATION_CRAFTING].value then GridSonifier.on_tick_per_player(pindex) end

      -- Combat sonification
      if mod_settings[SETTING_NAMES.SONIFICATION_COMBAT_ENEMIES].value then EnemyRadar.on_tick_per_player(pindex) end
      if mod_settings[SETTING_NAMES.SONIFICATION_COMBAT_SPAWNERS].value then SpawnerRadar.on_tick_per_player(pindex) end

      -- Health bar sonification
      HealthBar.on_tick_per_player(pindex)

      ::continue::
   end
end

-- The radars run only while their setting is on, so switching one off stops the sounds it was playing.
local RADAR_BY_SETTING = {
   [SETTING_NAMES.SONIFICATION_COMBAT_ENEMIES] = EnemyRadar,
   [SETTING_NAMES.SONIFICATION_COMBAT_SPAWNERS] = SpawnerRadar,
}

EventManager.on_event(defines.events.on_runtime_mod_setting_changed, function(event)
   local radar = RADAR_BY_SETTING[event.setting]
   -- No player when the change was not to a player's current setting
   local pindex = event.player_index
   if not radar or not pindex then return end
   if not game.get_player(pindex).mod_settings[event.setting].value then radar.stop_all(pindex) end
end)

return mod
