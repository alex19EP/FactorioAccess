--[[
Zoom.

The game's own zoom controls change the zoom (helper-scripts/factorio-access-keys.ps1 puts zoom in and
zoom out on EQUALS and MINUS). This reads it as the number of tiles across the screen and gives the
area the sound model searches.

The DLL says each zoom change, and the view the player enters with what is in hand: it runs on
frames, which go on while the map is paused (the map editor pauses it in single player), where
ticks stop.
]]
local Hand = require("scripts.hand")
local SoundModel = require("scripts.sound-model")

---@type fa.Native?
local native = rawget(_G, "fa_native")

local mod = {}

-- The base pixels per tile at zoom=1.
local base_pixels_per_tile = 32

-- The views said on entering them, by controller.
---@type table<defines.controllers, true?>
local NAMED_VIEWS = {
   [defines.controllers.remote] = true,
   [defines.controllers.character] = true,
   [defines.controllers.god] = true,
   [defines.controllers.editor] = true,
}

---Get the current zoom level in tiles.
---@param pindex integer
---@return integer tiles The tile count for the current zoom
function mod.get_current_zoom_tiles(pindex)
   local player = game.get_player(pindex)
   local screen = player.display_resolution
   local screen_dimension = math.max(screen.width, screen.height)
   return math.floor(screen_dimension / (player.zoom * base_pixels_per_tile * player.display_density_scale) + 0.5)
end

-- Map cells across the screen; see map-cells.lua.
local CELLS_PER_SCREEN = 30

---The size in tiles of a map cell on a screen `tiles` across.
---@param tiles number
---@return number A power of two, below 1 on a screen narrower than CELLS_PER_SCREEN tiles
function mod.map_cell_size_for(tiles)
   return 2 ^ math.floor(math.log(tiles / CELLS_PER_SCREEN, 2) + 0.5)
end

---The size in tiles of a map cell, by which the cursor moves on the full map. Nil off the full map.
---@param pindex integer
---@return integer?
function mod.get_map_cell_size(pindex)
   if game.get_player(pindex).render_mode ~= defines.render_mode.chart then return nil end
   -- The full map spans over 200 tiles, so a cell is at least 8
   return mod.map_cell_size_for(mod.get_current_zoom_tiles(pindex)) --[[@as integer]]
end

---While the view the player entered is still to be said, gives the DLL the hand to say with it and
---returns true; otherwise false, and the caller says the hand. For speech only.
---@param pindex integer
---@param hand LocalisedString?
---@return boolean
function mod.hand_for_view(pindex, hand)
   return native ~= nil and native.view_hand(pindex, hand)
end

---The DLL says the view the player entered, with the zoom it opens at and what is now in hand, once
---the zoom has settled.
---@param event EventData.on_player_controller_changed
function mod.on_controller_changed(event)
   local pindex = event.player_index
   if native and NAMED_VIEWS[game.get_player(pindex).controller_type] then
      native.view_entered(pindex, Hand.describe(pindex))
   end
end

---@class fa.Zoom.SearchArea
---@field left number
---@field top number
---@field right number
---@field bottom number
---@field half_width number

---Get the search area centered on the sound model reference point (cursor or character)
---@param pindex integer
---@return fa.Zoom.SearchArea
function mod.get_search_area(pindex)
   local ref_pos = SoundModel.get_reference_position(pindex)
   local tiles = mod.get_current_zoom_tiles(pindex)
   local half_tiles = tiles / 2

   return {
      left = ref_pos.x - half_tiles,
      top = ref_pos.y - half_tiles,
      right = ref_pos.x + half_tiles,
      bottom = ref_pos.y + half_tiles,
      half_width = half_tiles,
   }
end

return mod
