local NativeCursor = require("scripts.native-cursor")
local VanillaMode = require("scripts.vanilla-mode")

local mod = {}

function mod.on_tick()
   for _, p in pairs(game.players) do
      if NativeCursor.is_loaded() then
         -- The game's cursor follows the FA cursor, so vanilla selects what it is over.
         -- This writes player state from whether the DLL is loaded: in multiplayer every client
         -- must run the DLL, or none.
         p.game_view_settings.update_entity_selection = true
      elseif not VanillaMode.is_enabled(p.index) then
         p.game_view_settings.update_entity_selection = false
      end
   end
end

return mod
