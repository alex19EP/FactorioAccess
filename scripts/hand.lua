--[[
What the player holds: the item in hand, its facing direction where it has one, its count and the
total with the main inventory; or the ghost of an item, which remote view puts in hand instead.
]]
local Blueprints = require("scripts.blueprints")
local FaUtils = require("scripts.fa-utils")
local NativeCursor = require("scripts.native-cursor")
local Speech = require("scripts.speech")
local UpgradePlanner = require("scripts.upgrade-planner")

local mod = {}

---The facing direction of what `item` builds, as the two parameters of the hand descriptions: 1 and
---the direction where it has one, else 0 and nothing.
---@param pindex integer
---@param item LuaItemPrototype
---@return integer, LocalisedString
local function facing(pindex, item)
   local build_entity = item.place_result
   local direction = NativeCursor.build_direction(pindex)
   if build_entity and build_entity.supports_direction and direction then
      return 1, { "fa.facing-direction", FaUtils.direction_lookup(direction) }
   end
   return 0, ""
end

---Whether the player holds something that builds on the map: an item that places an entity, a
---rail planner, a blueprint or a book, from the inventory or the library, or the ghost of such an
---item. The I key detaches the cursor from it instead of anchoring the cursor to the character.
---Read from the game alone, so every client takes the key the same way.
---@param pindex integer
---@return boolean
function mod.holds_build(pindex)
   local player = game.get_player(pindex)
   local stack = player.cursor_stack
   if stack and stack.valid_for_read then
      local item = stack.prototype
      return stack.is_blueprint or stack.is_blueprint_book or item.place_result ~= nil or item.type == "rail-planner"
   end
   local ghost = player.cursor_ghost
   if ghost then
      local item = ghost.name --[[@as LuaItemPrototype]]
      return item.place_result ~= nil or item.type == "rail-planner"
   end
   local record = player.cursor_record
   return record ~= nil and (record.type == "blueprint" or record.type == "blueprint-book")
end

---Describes what the player holds. Nil when the hand is empty.
---@param pindex integer
---@return LocalisedString?
function mod.describe(pindex)
   local player = game.get_player(pindex)
   local cursor_stack = player.cursor_stack
   if cursor_stack and cursor_stack.valid_for_read then
      if cursor_stack.is_blueprint then return Blueprints.get_blueprint_info(cursor_stack, true, pindex) end
      if cursor_stack.is_blueprint_book then return Blueprints.get_blueprint_book_info(cursor_stack, true) end
      if cursor_stack.is_upgrade_item then
         local message = Speech.MessageBuilder.new()
         UpgradePlanner.describe_planner(message, cursor_stack)
         return message:build()
      end
      if cursor_stack.is_deconstruction_item then
         local label = cursor_stack.label
         if label and label ~= "" then return { "fa.item-decon-planner-labeled", label } end
         return { "item-name.deconstruction-planner" }
      end
      local has_direction, direction = facing(pindex, cursor_stack.prototype)
      -- No main inventory while the player has no character, as on a space platform or dead.
      local inventory = player.get_main_inventory()
      local extra = inventory and inventory.get_item_count(cursor_stack.name) or 0
      return {
         "fa.cursor-description",
         cursor_stack.prototype.localised_name,
         has_direction,
         direction,
         cursor_stack.count,
         extra > 0 and cursor_stack.count + extra or 0,
      }
   end

   local cursor_ghost = player.cursor_ghost
   if cursor_ghost then
      local item = cursor_ghost.name --[[@as LuaItemPrototype]]
      local has_direction, direction = facing(pindex, item)
      return { "fa.cursor-ghost-description", item.localised_name, has_direction, direction }
   end
   return nil
end

return mod
