--Here: mining functions, such as obstacle clearing
local Consts = require("scripts.consts")

local mod = {}

function mod.play_mining_sound(pindex)
   local player = game.players[pindex]
   if player and player.mining_state.mining and player.selected and player.selected.valid then
      if player.selected and player.selected.prototype.is_building then
         player.play_sound({ path = "player-mine" })
      elseif player.selected and player.selected.type ~= "resource" then
         player.play_sound({ path = "player-mine" }) --Mine other things, eg. character corpses, laterdo new sound
      end
   end
end

--Mines an entity with the right sound
function mod.try_to_mine_with_soun(ent, pindex)
   if ent ~= nil and ent.valid and ((ent.destructible and ent.type ~= "resource") or ent.name == "item-on-ground") then
      local ent_name = ent.name
      if
         game.get_player(pindex).mine_entity(ent, false) and helpers.is_valid_sound_path("entity-mined/" .. ent_name)
      then
         game.get_player(pindex).play_sound({ path = "entity-mined/" .. ent_name })
         return true
      else
         return false
      end
   end
end

--Mines all simple obstacles selected the selected area. This includes trees, rocks, remnants, and ground items. Useful when placing structures. Forces mining.
function mod.clear_obstacles_in_circle(position, radius, pindex)
   local surf = game.get_player(pindex).surface
   local comment = ""
   local trees_cleared = 0
   local rocks_cleared = 0
   local remnants_cleared = 0
   local ground_items_cleared = 0
   --Find and mine trees
   local trees = surf.find_entities_filtered({ position = position, radius = radius, type = "tree" })
   for i, tree_ent in ipairs(trees) do
      rendering.draw_circle({
         color = { 1, 0, 0 },
         radius = 1,
         width = 1,
         target = tree_ent.position,
         surface = tree_ent.surface,
         time_to_live = 60,
      })
      game.get_player(pindex).mine_entity(tree_ent, true)
      trees_cleared = trees_cleared + 1
   end

   --Find and mine rocks. Note that they are resource entities with specific names
   local resources = surf.find_entities_filtered({
      position = position,
      radius = radius,
      name = { "big-rock", "huge-rock", "big-sand-rock" },
   })
   for i, resource_ent in ipairs(resources) do
      if resource_ent ~= nil and resource_ent.valid then
         rendering.draw_circle({
            color = { 1, 0, 0 },
            radius = 2,
            width = 2,
            target = resource_ent.position,
            surface = resource_ent.surface,
            time_to_live = 60,
         })
         game.get_player(pindex).mine_entity(resource_ent, true)
         rocks_cleared = rocks_cleared + 1
      end
   end

   --Find and mine corpse entities such as building remnants
   local remnant_ents = surf.find_entities_filtered({
      position = position,
      radius = radius,
      name = Consts.ENT_NAMES_CLEARED_AS_OBSTACLES,
   })
   for i, remnant_ent in ipairs(remnant_ents) do
      if remnant_ent ~= nil and remnant_ent.valid then
         rendering.draw_circle({
            color = { 1, 0, 0 },
            radius = 2,
            width = 2,
            target = remnant_ent.position,
            surface = remnant_ent.surface,
            time_to_live = 60,
         })
         remnant_ent.destroy({})
         remnants_cleared = remnants_cleared + 1
      end
   end
   --game.get_player(pindex).print("remnants cleared: " .. remnants_cleared)--debug

   --Find and mine items on the ground
   local ground_items = surf.find_entities_filtered({ position = position, radius = 5, name = "item-on-ground" })
   for i, ground_item in ipairs(ground_items) do
      rendering.draw_circle({
         color = { 1, 0, 0 },
         radius = 0.25,
         width = 2,
         target = ground_item.position,
         surface = surf,
         time_to_live = 60,
      })
      game.get_player(pindex).mine_entity(ground_item, true)
      ground_items_cleared = ground_items_cleared + 1
   end

   --Report clear and pickup counts
   if trees_cleared + rocks_cleared + ground_items_cleared + remnants_cleared > 0 then
      comment = "cleared "
         .. trees_cleared
         .. " trees and "
         .. rocks_cleared
         .. " rocks and "
         .. remnants_cleared
         .. " remnants and "
         .. ground_items_cleared
         .. " ground items "
   end
   --Draw the area being cleared
   rendering.draw_circle({
      color = { 0, 1, 0 },
      radius = radius,
      width = radius,
      target = position,
      surface = surf,
      time_to_live = 60,
   })
   return (trees_cleared + rocks_cleared + remnants_cleared + ground_items_cleared), comment
end

return mod
