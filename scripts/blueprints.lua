--Here: Functions related to ghosts, blueprints and blueprint books
--Does not include event handlers

local BuildDimensions = require("scripts.build-dimensions")
local BuildingTools = require("scripts.building-tools")
local FaUtils = require("scripts.fa-utils")
local Graphics = require("scripts.graphics")
local Localising = require("scripts.localising")
local Speech = require("scripts.speech")
local MessageBuilder = Speech.MessageBuilder
local PlayerMiningTools = require("scripts.player-mining-tools")
local UiRouter = require("scripts.ui.router")
local Viewpoint = require("scripts.viewpoint")

local dirs = defines.direction

local mod = {}

--todo cleanup blueprint calls in control.lua so that blueprint data editing calls happen only within this module

function mod.get_blueprint_label(stack)
   if stack.is_blueprint_book then
      -- For blueprint books, get the label of the active blueprint
      local book_inv = stack.get_inventory(defines.inventory.item_main)
      if book_inv and stack.active_index then
         local active_bp = book_inv[stack.active_index]
         if active_bp and active_bp.valid_for_read then return active_bp.label or "no name" end
      end
      return "no name"
   elseif stack.is_blueprint then
      return stack.label or "no name"
   end
   return "no name"
end

function mod.get_bp_data_for_edit(stack)
   ---@diagnostic disable-next-line: param-type-mismatch
   return helpers.json_to_table(helpers.decode_string(string.sub(stack.export_stack(), 2)))
end

--Works for blueprints and also books too
function mod.set_stack_bp_from_data(stack, bp_data)
   stack.import_stack("0" .. helpers.encode_string(helpers.table_to_json(bp_data)))
end

--Create a blueprint from a rectangle between any two points and give it to the player's hand
function mod.create_blueprint(pindex, point_1, point_2, prior_bp_data)
   local top_left, bottom_right = FaUtils.get_top_left_and_bottom_right(point_1, point_2)
   local p = game.get_player(pindex)
   if prior_bp_data ~= nil then
      --First clear the bp in hand
      p.cursor_stack.set_stack({ name = "blueprint", count = 1 })
   end
   if
      not p.cursor_stack.valid_for_read
      or p.cursor_stack.valid_for_read
         and not (p.cursor_stack.is_blueprint and p.cursor_stack.is_blueprint_setup() == false and prior_bp_data == nil)
   then
      local cleared = p.clear_cursor()
      if not cleared then
         Speech.speak(pindex, { "fa.blueprints-error-cursor-full" })
         return
      end
   end
   p.cursor_stack.set_stack({ name = "blueprint" })
   p.cursor_stack.create_blueprint({ surface = p.surface, force = p.force, area = { top_left, bottom_right } })

   --Avoid empty blueprints
   local ent_count = p.cursor_stack.get_blueprint_entity_count()
   if ent_count == 0 then
      if prior_bp_data == nil then p.cursor_stack.set_stack({ name = "blueprint" }) end
      local message = MessageBuilder.new()
      message:fragment({ "fa.blueprints-selection-empty" })
      if prior_bp_data ~= nil then message:fragment({ "fa.blueprints-keeping-old" }) end
      Speech.speak(pindex, message:build())
   else
      local prior_name = ""
      if prior_bp_data ~= nil then prior_name = prior_bp_data.blueprint.label or "" end
      local message = MessageBuilder.new()
      message:fragment({ "fa.blueprints-created", prior_name })
      message:fragment(FaUtils.format_count(ent_count, { "fa.blueprints-entities" }))
      message:fragment({ "fa.blueprints-in-hand" })
      Speech.speak(pindex, message:build())
   end

   --Copy label and description and icons from previous version
   if prior_bp_data ~= nil then
      local bp_data = mod.get_bp_data_for_edit(p.cursor_stack)
      bp_data.blueprint.label = prior_bp_data.blueprint.label or ""
      bp_data.blueprint.label_color = prior_bp_data.blueprint.label_color or { 1, 1, 1 }
      bp_data.blueprint.description = prior_bp_data.blueprint.description or ""
      bp_data.blueprint.icons = prior_bp_data.blueprint.icons or {}
      if ent_count == 0 then bp_data.blueprint.entities = prior_bp_data.blueprint.entities end
      mod.set_stack_bp_from_data(p.cursor_stack, bp_data)
   end

   --Use this opportunity to update saved information about the blueprint's corners (used when drawing the footprint)
   local width, height = BuildDimensions.get_stack_build_dimensions(p.cursor_stack, dirs.north)
   if width and height then
      storage.players[pindex].blueprint_width_in_hand = width + 1
      storage.players[pindex].blueprint_height_in_hand = height + 1
   end
end

--Export and import the same blueprint so that its parameters reset, e.g. rotation.
function mod.refresh_blueprint_in_hand(pindex)
   local p = game.get_player(pindex)
   if p.cursor_stack.is_blueprint_setup() == false then return end
   local bp_data = mod.get_bp_data_for_edit(p.cursor_stack)
   mod.set_stack_bp_from_data(p.cursor_stack, bp_data)
end

--Basic info for when the blueprint item is read.
function mod.get_blueprint_info(stack, in_hand, pindex)
   --Not a blueprint
   if stack.is_blueprint == false then return "" end
   --Empty blueprint
   if not stack.is_blueprint_setup() then return { "fa.blueprints-empty" } end

   local message = MessageBuilder.new()

   --Get name
   local name = mod.get_blueprint_label(stack)
   if name == nil then name = "" end

   --Add intro
   if in_hand then
      message:fragment({ "fa.blueprints-name-in-hand-features", name })
   else
      message:fragment({ "fa.blueprints-name-features", name })
   end

   --Use icons as extra info (in case it is not named)
   local icons = stack.preview_icons
   if icons == nil or #icons == 0 then
      message:fragment({ "fa.blueprints-no-details" })
      return message:build()
   end

   for i, signal in ipairs(icons) do
      if signal.index > 1 then message:fragment({ "fa.blueprints-and" }) end

      if signal.signal.name ~= nil then
         local proto = FaUtils.find_prototype(signal.signal.name)
         if proto then
            message:fragment(Localising.get_localised_name_with_fallback(proto))
         else
            message:fragment(signal.signal.name)
         end
      else
         message:fragment({ "fa.blueprints-unknown-icon" })
      end
   end

   message:fragment({ "fa.blueprints-entities-total", tostring(stack.get_blueprint_entity_count()) })

   --Use this opportunity to update saved information about the blueprint's corners (used when drawing the footprint)
   if in_hand then
      local width, height = BuildDimensions.get_stack_build_dimensions(stack, dirs.north)
      if width and height then
         storage.players[pindex].blueprint_width_in_hand = width + 1
         storage.players[pindex].blueprint_height_in_hand = height + 1
      end
   end

   return message:build()
end

function mod.get_blueprint_icons_info(bp_table)
   local result = { "" }
   --Use icons as extra info (in case it is not named)
   local icons = bp_table.icons
   if icons == nil or #icons == 0 then
      result = { "", result, { "fa.blueprints-no-icons" } }
      return result
   end

   for i, signal in ipairs(icons) do
      if signal.index > 1 then result = { "", result, { "fa.blueprints-and" } } end
      if signal.signal.name ~= nil then
         result = { "", result, signal.signal.name }
      else
         result = { "", result, { "fa.blueprints-unknown-icon" } }
      end
   end
   return result
end

function mod.apply_blueprint_import(pindex, text)
   local bp = game.get_player(pindex).cursor_stack
   --local result = bp.import_stack("0"..text)
   local result = bp.import_stack(text)
   if result == 0 then
      if bp.is_blueprint then
         Speech.speak(pindex, { "fa.blueprints-imported-successfully", mod.get_blueprint_label(bp) })
      elseif bp.is_blueprint_book then
         Speech.speak(pindex, { "fa.blueprints-imported-book" })
      else
         Speech.speak(pindex, { "fa.blueprints-imported-unknown" })
      end
   elseif result == -1 then
      if bp.is_blueprint then
         Speech.speak(pindex, { "fa.blueprints-imported-with-errors", mod.get_blueprint_label(bp) })
      elseif bp.is_blueprint_book then
         Speech.speak(pindex, { "fa.blueprints-imported-book-errors" })
      else
         Speech.speak(pindex, { "fa.blueprints-imported-unknown-errors" })
      end
   else --result == 1
      Speech.speak(pindex, { "fa.blueprints-import-failed" })
   end
end

--Basic info for when the blueprint book item is read (using inventory API)
function mod.get_blueprint_book_info(stack, in_hand)
   if not stack or not stack.is_blueprint_book then return "" end

   local label = stack.label or "unnamed book"
   local book_inv = stack.get_inventory(defines.inventory.item_main)
   local item_count = book_inv and #book_inv or 0

   local result = { "" }
   if in_hand then
      table.insert(result, { "fa.blueprints-book-label-in-hand", label })
   else
      table.insert(result, { "fa.blueprints-book-label", label })
   end
   table.insert(result, ", ")
   table.insert(result, { "fa.blueprints-with-items", tostring(item_count) })

   return result
end

return mod
