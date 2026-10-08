--[[
Says a ghost the player placed by hand: in remote view the hand holds only ghosts, and the game
marks the place silently. A blueprint's ghosts and those of a drag along the build key are not
said, like the buildings a drag builds.
]]
local NativeCursor = require("scripts.native-cursor")
local Speech = require("scripts.speech")

local mod = {}

---@param event EventData.on_built_entity
function mod.on_built_entity(event)
   local entity = event.entity
   if entity.type ~= "entity-ghost" and entity.type ~= "tile-ghost" then return end
   local pindex = event.player_index
   if game.get_player(pindex).is_cursor_blueprint() or NativeCursor.drag_build(pindex) then return end
   Speech.speak(pindex, { "fa.building-placed-ghost", entity.ghost_localised_name })
end

return mod
