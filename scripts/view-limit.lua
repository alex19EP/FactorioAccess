--[[
The cursor reaches what a sighted player's mouse reaches.

In the character view the camera stays on the character, so the mouse can point only at the tiles on
the screen around it. The DLL keeps the cursor on those tiles (native/src/cursor.h): the movement
keys stop at the edge, walking or zooming in pulls the cursor back, and a jump to anything off the
screen opens remote view there. In remote view the camera follows the cursor, and in the map editor
and god mode (the sandbox, no character) too (mod.follow_cursor): a sighted player flies those.

This module answers the same question for the moves the mod makes itself, such as the skip keys, so
that they stop at the edge too. The zoom and the screen size are part of the game state, so every
peer answers alike.
]]
local Speech = require("scripts.speech")

local mod = {}

-- Pixels per tile at zoom 1.
local PIXELS_PER_TILE = 32

-- Controllers whose camera the mod moves onto the cursor: they fly where the cursor goes.
---@type table<defines.controllers, true?>
local FOLLOWS_CURSOR = {
   [defines.controllers.editor] = true,
   [defines.controllers.god] = true,
}

---The tiles wholly on the screen in the character view, as left, top, right and bottom tile
---coordinates, all inclusive. Nil where the cursor is not limited: in remote view, the map editor,
---god mode and other controllers the camera moves freely.
---@param player LuaPlayer
---@return integer?, integer?, integer?, integer?
local function screen_tiles(player)
   if player.controller_type ~= defines.controllers.character then return nil end
   local scale = player.zoom * PIXELS_PER_TILE * player.display_density_scale
   local half_width = player.display_resolution.width / scale / 2
   local half_height = player.display_resolution.height / scale / 2
   local center = player.position
   return math.ceil(center.x - half_width),
      math.ceil(center.y - half_height),
      math.floor(center.x + half_width) - 1,
      math.floor(center.y + half_height) - 1
end

---Whether the cursor may stand on the tile holding `position`: on the screen in the character
---view, anywhere elsewhere.
---@param pindex integer
---@param position fa.Point
---@return boolean
function mod.allows(pindex, position)
   local left, top, right, bottom = screen_tiles(assert(game.get_player(pindex)))
   if not left then return true end
   local x, y = math.floor(position.x), math.floor(position.y)
   return x >= left and x <= right and y >= top and y <= bottom
end

---In the map editor and god mode the camera follows the cursor, as in remote view: after a key, the
---editor or the god moves onto the cursor's tile when the cursor left the camera's, as a sighted
---player flies it there. The game takes no map drag in the editor, so the DLL cannot pan it. The
---tile comes from the key's event, the same on every peer. The keys that fly the editor or the god
---still do, and the DLL brings the cursor along.
---@param pindex integer
---@param tile fa.Tile
function mod.follow_cursor(pindex, tile)
   local player = assert(game.get_player(pindex))
   if not FOLLOWS_CURSOR[player.controller_type] then return end
   local position = player.position --[[@as MapPosition.struct]]
   if math.floor(position.x) == tile.x and math.floor(position.y) == tile.y then return end
   player.teleport({ tile.x + 0.5, tile.y + 0.5 })
end

---For movement keys that cannot go further: says the cursor is at the edge of the screen.
---@param pindex integer
function mod.say_edge(pindex)
   game.get_player(pindex).play_sound({ path = "inventory-edge" })
   Speech.speak(pindex, { "fa.cursor-view-edge" })
end

return mod
