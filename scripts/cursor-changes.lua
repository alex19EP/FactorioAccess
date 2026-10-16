--- Handles cursor stack changes
local mod = {}

local Blueprints = require("scripts.blueprints")

local dirs = defines.direction

--- Cursor stack changed handler
---@param event EventData.on_player_cursor_stack_changed
---@param pindex integer
---@param read_hand function
function mod.on_cursor_stack_changed(event, pindex, read_hand)
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
      end
   end

   if storage.players[pindex].previous_hand_item_name ~= new_item_name then
      storage.players[pindex].previous_hand_item_name = new_item_name
      read_hand(pindex)
   end
end

return mod
