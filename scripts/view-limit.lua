--[[
The cursor reaches what a sighted player's mouse reaches.

In the character view the camera stays on the character, so the mouse can point only at the tiles on
the screen around it, and the FA cursor is kept on those tiles: movement keys stop at the edge, and
walking or zooming in pulls an unanchored cursor back onto the screen (an anchored cursor goes back
to the character). To look further a sighted player opens remote view, and so does the cursor: a
jump to anything off the screen opens remote view there. In remote view the camera stays centred on
the cursor, so the game selects and builds under it, and leaving remote view brings the cursor back
to the character as the game brings the camera back.

God mode (the sandbox, no character) is the same: its camera stays on the god's position, which
moves only by walking, so the cursor stays on the screen around it, and leaving remote view brings
the cursor back there.

The zoom and the screen size are part of the game state, so every peer limits the cursor alike.
]]
local Speech = require("scripts.speech")
local Viewpoint = require("scripts.viewpoint")

local mod = {}

-- Pixels per tile at zoom 1.
local PIXELS_PER_TILE = 32

-- Controllers whose camera stays on the player's own position.
local FIXED_CAMERA = {
   [defines.controllers.character] = true,
   [defines.controllers.god] = true,
}

---The tiles wholly on the screen in the character or god view, as left, top, right and bottom tile
---coordinates, all inclusive. Nil where the cursor is not limited: in remote view, the map editor
---and other controllers the camera moves freely.
---@param player LuaPlayer
---@return integer?, integer?, integer?, integer?
local function screen_tiles(player)
   if not FIXED_CAMERA[player.controller_type] then return nil end
   local scale = player.zoom * PIXELS_PER_TILE * player.display_density_scale
   local half_width = player.display_resolution.width / scale / 2
   local half_height = player.display_resolution.height / scale / 2
   local center = player.position
   return math.ceil(center.x - half_width),
      math.ceil(center.y - half_height),
      math.floor(center.x + half_width) - 1,
      math.floor(center.y + half_height) - 1
end

---Whether the cursor may stand on the tile holding `position`: anywhere in remote view, on the
---screen in the character and god views.
---@param pindex integer
---@param position fa.Point
---@return boolean
function mod.allows(pindex, position)
   local left, top, right, bottom = screen_tiles(game.get_player(pindex))
   if not left then return true end
   local x, y = math.floor(position.x), math.floor(position.y)
   return x >= left and x <= right and y >= top and y <= bottom
end

---For movement keys that cannot go further: says the cursor is at the edge of the screen.
---@param pindex integer
function mod.say_edge(pindex)
   game.get_player(pindex).play_sound({ path = "inventory-edge" })
   Speech.speak(pindex, { "fa.cursor-view-edge" })
end

---@param position fa.Point
---@return MapPosition
local function tile_centre(position)
   return { x = position.x + 0.5, y = position.y + 0.5 }
end

---@param pindex integer
---@param position fa.Point
local function on_cursor_moved(pindex, position)
   local player = game.get_player(pindex)
   if player.controller_type == defines.controllers.remote then
      player.teleport(tile_centre(position))
   elseif not mod.allows(pindex, position) then
      player.set_controller({ type = defines.controllers.remote, position = tile_centre(position) })
   end
end

Viewpoint.register_listener("cursor_moved", on_cursor_moved)

---Remote view opened at the character, as the map key opens it, comes to the cursor; opened
---elsewhere, as an alert, a pin or the map search opens it, it brings the cursor along. Leaving
---remote view puts the cursor on the character, or in god mode on the god's position.
---@param event EventData.on_player_controller_changed
function mod.on_controller_changed(event)
   local player = game.get_player(event.player_index)
   local vp = Viewpoint.get_viewpoint(event.player_index)
   if player.controller_type == defines.controllers.remote then
      local camera, character = player.position, player.physical_position
      if math.abs(camera.x - character.x) < 1 and math.abs(camera.y - character.y) < 1 then
         player.teleport(tile_centre(vp:get_cursor_pos()))
      else
         vp:set_cursor_pos(camera)
      end
   elseif event.old_type == defines.controllers.remote and FIXED_CAMERA[player.controller_type] then
      vp:set_cursor_pos(player.position)
   end
end

---Keeps each cursor in the character or god view on the screen as the player walks and the zoom
---changes. In remote view the camera stays on the cursor, so a camera the game moved (to an alert, a
---pin, a search result, an entity it follows) brings the cursor along.
function mod.on_tick()
   for _, player in pairs(game.connected_players) do
      local left, top, right, bottom = screen_tiles(player)
      if player.controller_type == defines.controllers.remote then
         local vp = Viewpoint.get_viewpoint(player.index)
         local cursor, camera = vp:get_cursor_pos(), player.position
         if cursor.x ~= math.floor(camera.x) or cursor.y ~= math.floor(camera.y) then vp:set_cursor_pos(camera) end
      elseif left then
         local vp = Viewpoint.get_viewpoint(player.index)
         local pos = vp:get_cursor_pos()
         if pos.x < left or pos.x > right or pos.y < top or pos.y > bottom then
            if vp:get_cursor_anchored() then
               vp:set_cursor_pos(player.position)
            else
               vp:set_cursor_pos({
                  x = math.min(math.max(pos.x, left), right),
                  y = math.min(math.max(pos.y, top), bottom),
               })
            end
         end
      end
   end
end

return mod
