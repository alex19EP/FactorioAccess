--[[
Zoom.

The game's own zoom controls change the zoom (helper-scripts/bind-mouse-keys.ps1 puts zoom in and
zoom out on EQUALS and MINUS). This reads it as the number of tiles across the screen, speaks each
change, and gives the area the sound model searches.
]]
local Hand = require("scripts.hand")
local SoundModel = require("scripts.sound-model")
local Speech = require("scripts.speech")

local mod = {}

-- The base pixels per tile at zoom=1.
local base_pixels_per_tile = 32

-- The zoom each player last heard, so each change is spoken once. It only drives speech, so it may
-- differ between peers.
---@type table<integer, number>
local spoken_zoom = {}
-- The zoom seen on the previous tick: a change is spoken once it has held for a tick, when the
-- render mode has caught up with it and a burst of wheel steps has ended.
---@type table<integer, number>
local seen_zoom = {}
-- Views each player entered and has yet to hear, with the zoom they open at.
---@type table<integer, "remote"|"character">
local entered_view = {}

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
---@return integer
function mod.map_cell_size_for(tiles)
   return 2 ^ math.floor(math.log(tiles / CELLS_PER_SCREEN, 2) + 0.5)
end

---The size in tiles of a map cell, by which the cursor moves on the full map. Nil off the full map.
---@param pindex integer
---@return integer?
function mod.get_map_cell_size(pindex)
   if game.get_player(pindex).render_mode ~= defines.render_mode.chart then return nil end
   return mod.map_cell_size_for(mod.get_current_zoom_tiles(pindex))
end

---Append zoom info to a MessageBuilder
---@param pindex integer
---@param mb fa.MessageBuilder
function mod.append_zoom_info(pindex, mb)
   local tiles = mod.get_current_zoom_tiles(pindex)
   mb:fragment({ "fa.zoom-current", tiles })
end

---Whether the player entered a view they have yet to hear about.
---@param pindex integer
---@return boolean
function mod.view_pending(pindex)
   return entered_view[pindex] ~= nil
end

---Says the view the player entered, with the zoom it opens at and what is now in hand, once the zoom
---has settled.
---@param event EventData.on_player_controller_changed
function mod.on_controller_changed(event)
   local controller = game.get_player(event.player_index).controller_type
   if controller == defines.controllers.remote then
      entered_view[event.player_index] = "remote"
   elseif controller == defines.controllers.character then
      entered_view[event.player_index] = "character"
   end
end

---Speaks each player's zoom when it changed since they last heard it, and the view they entered. The
---first check after a load only records the zoom.
function mod.on_tick()
   for _, player in pairs(game.connected_players) do
      local pindex = player.index
      local zoom = player.zoom
      local previous = seen_zoom[pindex]
      seen_zoom[pindex] = zoom
      local last = spoken_zoom[pindex]
      if not last then spoken_zoom[pindex] = zoom end
      local view = entered_view[pindex]
      if previous == zoom and (view or last and math.abs(zoom - last) > 1e-6) then
         spoken_zoom[pindex] = zoom
         entered_view[pindex] = nil
         local tiles = mod.get_current_zoom_tiles(pindex)
         -- 0 off the full map
         local cell_size = mod.get_map_cell_size(pindex) or 0
         if view then
            local message = Speech.MessageBuilder.new()
            message:fragment({ "fa.zoom-view-" .. view, tiles, cell_size })
            local hand = Hand.describe(pindex)
            if hand then message:list_item(hand) end
            Speech.speak(pindex, message:build())
         else
            Speech.speak(pindex, { "fa.zoom-set", tiles, cell_size })
         end
      end
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
