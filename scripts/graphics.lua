--Here: Mod GUI and graphics drawing
--Note: Does not include every single rendering call made by the mod, such as circles being drawn by obstacle clearing.

local FaUtils = require("scripts.fa-utils")
local UiRouter = require("scripts.ui.router")
local Viewpoint = require("scripts.viewpoint")

local mod = {}

--Updates graphics to match the item in hand: the paving area of tiles and the selection box of
--planners. The game draws the build preview of entities and blueprints itself, at the world cursor
--that follows the mod's cursor.
function mod.sync_build_cursor_graphics(pindex)
   local player = storage.players[pindex]
   if player == nil or player.player.character == nil then return end
   local stack = game.get_player(pindex).cursor_stack
   turn_to_cursor_direction_cardinal(pindex)
   local vp = Viewpoint.get_viewpoint(pindex)
   local cursor_pos = vp:get_cursor_pos()
   local cursor_size = vp:get_cursor_size()
   local holding = stack ~= nil and stack.valid_for_read
   if holding and stack.prototype.place_as_tile_result then
      --Tile placement preview
      local left_top = {
         math.floor(cursor_pos.x) - cursor_size,
         math.floor(cursor_pos.y) - cursor_size,
      }
      local right_bottom = {
         math.floor(cursor_pos.x) + cursor_size + 1,
         math.floor(cursor_pos.y) + cursor_size + 1,
      }
      mod.draw_large_cursor(left_top, right_bottom, pindex, { r = 0.25, b = 0.25, g = 1.0, a = 0.75 })
   elseif
      holding
      and (stack.is_blueprint or stack.is_deconstruction_item or stack.is_upgrade_item or stack.prototype.type == "selection-tool" or stack.prototype.type == "copy-paste-tool")
      and storage.players[pindex].bp_selecting == true
   then
      --Draw planner rectangles
      local top_left, bottom_right =
         FaUtils.get_top_left_and_bottom_right(storage.players[pindex].bp_select_point_1, cursor_pos)
      local color = { 1, 1, 1 }
      if stack.is_blueprint then
         color = { r = 0.25, b = 1.00, g = 0.50, a = 0.75 }
      elseif stack.is_deconstruction_item then
         color = { r = 1.00, b = 0.25, g = 0.50, a = 0.75 }
      elseif stack.is_upgrade_item then
         color = { r = 0.25, b = 0.25, g = 1.00, a = 0.75 }
      end
      if player.building_footprint ~= nil then player.building_footprint.destroy() end
      player.building_footprint = rendering.draw_rectangle({
         color = color,
         width = 2,
         surface = game.get_player(pindex).surface,
         left_top = top_left,
         right_bottom = bottom_right,
         draw_on_ground = false,
         players = nil,
      })
      player.building_footprint.visible = true
   elseif player.building_footprint ~= nil then
      player.building_footprint.visible = false
   end

   --Recolor cursor boxes if multiplayer
   if game.is_multiplayer() then mod.set_cursor_colors_to_player_colors(pindex) end
end

--Draws the mod cursor box and highlights an entity selected by the cursor.
function mod.draw_cursor_highlight(pindex, ent, box_type)
   local p = game.get_player(pindex)
   local vp = Viewpoint.get_viewpoint(pindex)
   local c_pos = vp:get_cursor_pos()
   local cursor_hidden = vp:get_cursor_hidden()
   local h_box = vp:get_cursor_ent_highlight_box()
   local h_tile = vp:get_cursor_tile_highlight_box()
   if c_pos == nil then return end
   if h_box ~= nil and h_box.valid then h_box.destroy() end
   if h_tile ~= nil and h_tile.valid then h_tile.destroy() end

   --Skip drawing if hide cursor is enabled
   if cursor_hidden then
      vp:set_cursor_ent_highlight_box(nil)
      vp:set_cursor_tile_highlight_box(nil)
      return
   end

   --Draw highlight box
   if ent ~= nil and ent.valid and ent.name ~= "highlight-box" and (p.selected == nil or p.selected.valid == false) then
      h_box = p.surface.create_entity({
         name = "highlight-box",
         force = "neutral",
         surface = p.surface,
         render_player_index = pindex,
         box_type = "entity",
         position = c_pos,
         source = ent,
      })
      if box_type ~= nil then
         h_box.highlight_box_type = box_type
      else
         h_box.highlight_box_type = "entity"
      end
   end

   --Highlight the currently focused ground tile.
   if math.floor(c_pos.x) == math.ceil(c_pos.x) then c_pos.x = c_pos.x + 0.01 end
   if math.floor(c_pos.y) == math.ceil(c_pos.y) then c_pos.y = c_pos.y + 0.01 end
   h_tile = rendering.draw_rectangle({
      color = { 0.75, 1, 1, 0.75 },
      surface = p.surface,
      draw_on_ground = true,
      players = nil,
      left_top = { math.floor(c_pos.x) + 0.05, math.floor(c_pos.y) + 0.05 },
      right_bottom = { math.ceil(c_pos.x) - 0.05, math.ceil(c_pos.y) - 0.05 },
   })

   vp:set_cursor_ent_highlight_box(h_box)
   vp:set_cursor_tile_highlight_box(h_tile)

   --Recolor cursor boxes if multiplayer
   if game.is_multiplayer() then mod.set_cursor_colors_to_player_colors(pindex) end
end

--Redraws the player's cursor highlight box as a rectangle around the defined area.
function mod.draw_large_cursor(input_left_top, input_right_bottom, pindex, colour_in)
   local vp = Viewpoint.get_viewpoint(pindex)
   local h_tile = vp:get_cursor_tile_highlight_box()
   if h_tile ~= nil then h_tile.destroy() end
   local colour = { 0.75, 1, 1 }
   if colour_in ~= nil then colour = colour_in end
   h_tile = rendering.draw_rectangle({
      color = colour,
      surface = game.get_player(pindex).surface,
      left_top = input_left_top,
      right_bottom = input_right_bottom,
      draw_on_ground = true,
      players = nil,
   })
   h_tile.visible = true
   vp:set_cursor_tile_highlight_box(h_tile)

   --Recolor cursor boxes if multiplayer
   if game.is_multiplayer() then mod.set_cursor_colors_to_player_colors(pindex) end
end

--Recolors the mod cursor box to match the player's color. Useful in multiplayer when multiple cursors are on screen.
function mod.set_cursor_colors_to_player_colors(pindex)
   if not check_for_player(pindex) then return end
   local p = game.get_player(pindex)
   local vp = Viewpoint.get_viewpoint(pindex)
   local h_tile = vp:get_cursor_tile_highlight_box()
   if h_tile ~= nil and h_tile.valid then h_tile.color = p.color end
   local footprint = storage.players[pindex].building_footprint
   if footprint ~= nil and footprint.valid then footprint.color = p.color end
end

return mod
