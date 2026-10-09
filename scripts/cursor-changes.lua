--- Handles cursor stack changes and pipette tool
local mod = {}

local Consts = require("scripts.consts")
local Viewpoint = require("scripts.viewpoint")
local Graphics = require("scripts.graphics")
local Blueprints = require("scripts.blueprints")
local BuildDimensions = require("scripts.build-dimensions")

local dirs = defines.direction

--- Pipette tool handler for fa-q key. The game's own pipette runs too and takes the entity's
--- direction as its build direction.
---@param event EventData.CustomInputEvent
function mod.kb_pipette_tool(event)
   local pindex = event.player_index
   local p = game.get_player(pindex)

   if p.is_cursor_empty() then
      local ent = p.selected
      if ent and ent.valid then p.pipette(ent.prototype) end
   end
end

--- Cursor stack changed handler
---@param event EventData.on_player_cursor_stack_changed
---@param pindex integer
---@param read_hand function
function mod.on_cursor_stack_changed(event, pindex, read_hand)
   local vp = Viewpoint.get_viewpoint(pindex)

   local player = game.get_player(pindex)
   local stack = player.cursor_stack
   local new_item_name = ""
   -- A ghost in hand, as remote view's ghost cursor selection puts there, is told apart from the item.
   local ghost = player.cursor_ghost
   if ghost then new_item_name = "ghost " .. ghost.name.name end
   if stack and stack.valid_for_read then
      new_item_name = stack.name
      if stack.is_blueprint and storage.players[pindex].blueprint_hand_direction ~= dirs.north then
         storage.players[pindex].blueprint_hand_direction = dirs.north
         if game.get_player(pindex).cursor_stack_temporary == false then
            Blueprints.refresh_blueprint_in_hand(pindex)
         end
         local width, height = BuildDimensions.get_stack_build_dimensions(stack, dirs.north)
         if width == nil or height == nil then return end
         storage.players[pindex].blueprint_width_in_hand = width + 1
         storage.players[pindex].blueprint_height_in_hand = height + 1
      end
   end

   if storage.players[pindex].previous_hand_item_name ~= new_item_name then
      storage.players[pindex].previous_hand_item_name = new_item_name

      vp:set_cursor_rotation_offset(0)

      read_hand(pindex)
   end

   storage.players[pindex].bp_selecting = false
   storage.players[pindex].blueprint_reselecting = false
   storage.players[pindex].ghost_rail_planning = false
   Graphics.sync_build_cursor_graphics(pindex)
end

return mod
