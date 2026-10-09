--[[
The cursor reaches what a sighted player's mouse reaches.

In the character view the camera stays on the character, so the mouse can point only at the tiles on
the screen around it. The DLL keeps the cursor on those tiles (native/src/cursor.h): the movement
keys stop at the edge, walking or zooming in pulls the cursor back, and a jump to anything off the
screen opens remote view there. In remote view the camera follows the cursor.

God mode (the sandbox, no character) is the same: its camera stays on the god's position.

This module answers the same question for the moves the mod makes itself, such as the skip keys, so
that they stop at the edge too. The zoom and the screen size are part of the game state, so every
peer answers alike.
]]
local Speech = require("scripts.speech")

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

return mod
