--[[
Player Initialization Module
Handles initialization of player state when joining the game.
]]

local mod = {}

---Initialize the globally saved data tables for a specific player
---@param player LuaPlayer
function mod.initialize(player)
   storage.players[player.index] = storage.players[player.index] or {}
   local faplayer = storage.players[player.index]

   -- The game draws its own build preview now; take down the arrow older saves still show
   if type(faplayer.building_dir_arrow) == "userdata" and faplayer.building_dir_arrow.valid then
      faplayer.building_dir_arrow.destroy()
   end
   faplayer.building_dir_arrow = nil

   faplayer.previous_hand_item_name = faplayer.previous_hand_item_name or ""
end

return mod
