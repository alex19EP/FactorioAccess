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
      local extra = player.get_main_inventory().get_item_count(cursor_stack.name)
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
