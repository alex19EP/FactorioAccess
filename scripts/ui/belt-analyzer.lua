--[[
What a transport belt carries, beside the game's own belt window (see entity-views.lua). The game
shows a belt's items only on the map, so four views show them: this belt's slots, and what the whole
belt, the belts feeding it and the belts it feeds carry on each lane, a column per lane.
]]
local EntityViews = require("scripts.ui.entity-views")
local FaUtils = require("scripts.fa-utils")
local Geometry = require("scripts.geometry")
local ItemInfo = require("scripts.item-info")
local Speech = require("scripts.speech")
local MessageBuilder = Speech.MessageBuilder
local TH = require("scripts.table-helpers")
local TransportBelts = require("scripts.transport-belts")

local mod = {}

---@param entity LuaEntity
---@param lane 1|2 left or right, facing along the belt
---@return LocalisedString
local function lane_name(entity, lane)
   local facing = entity.direction
   local side = lane == 1 and Geometry.dir_counterclockwise_90(facing) or Geometry.dir_clockwise_90(facing)
   return { "fa.ui-belt-analyzer-lane", FaUtils.direction_lookup(side) }
end

-- What one slot of a lane holds, then which slot it is.
---@param contents fa.TransportBelts.SlotBucket[][]
---@param lane 1|2
---@param slot integer
---@return LocalisedString
local function slot_cell(contents, lane, slot)
   local message = MessageBuilder.new()
   local bucket = contents[lane][slot]
   if bucket and next(bucket.items) then
      for name, quals in pairs(bucket.items) do
         for quality, count in pairs(quals) do
            message:list_item(ItemInfo.item_info({ name = name, quality = quality, count = count }))
         end
      end
   else
      message:list_item({ "fa.ui-belt-analyzer-empty" })
   end
   message:list_item({ "fa.ui-belt-analyzer-slot", slot })
   return message:build()
end

---@param entity LuaEntity
---@param node fa.TransportBelts.Node
---@return fa.EntityViews.View
local function slots_view(entity, node)
   local contents = node:get_all_contents()
   local columns = {}
   for lane = 1, 2 do
      local cells = {}
      for slot = 1, 4 do
         cells[slot] = slot_cell(contents, lane, slot)
      end
      columns[lane] = { title = lane_name(entity, lane), cells = cells }
   end
   return { title = { "fa.ui-belt-analyzer-tab-local" }, columns = columns }
end

-- Each lane's items, most first, with their share of the lane's length.
---@param entity LuaEntity
---@param title LocalisedString
---@param lanes fa.NQC[] left lane, then right
---@param lengths fa.TransportBelts.LaneLengths
---@return fa.EntityViews.View
local function totals_view(entity, title, lanes, lengths)
   if not next(lanes[1]) and not next(lanes[2]) then
      return { title = title, columns = { { cells = { { "fa.ui-belt-analyzer-no-contents" } } } } }
   end
   local columns = {}
   for lane = 1, 2 do
      local length = lengths[lane == 1 and "left" or "right"]
      local cells = {}
      for _, entry in ipairs(TH.nqc_to_sorted_descending(lanes[lane])) do
         local percent = length > 0 and string.format("%.1f", 100 * entry.count / length) or "0.0"
         table.insert(cells, { "fa.ui-belt-analyzer-aggregation", ItemInfo.item_info(entry), percent })
      end
      if not cells[1] then cells[1] = { "fa.ui-belt-analyzer-empty" } end
      columns[lane] = { title = lane_name(entity, lane), cells = cells }
   end
   return { title = title, columns = columns }
end

---@param entity LuaEntity a transport belt
---@return fa.EntityViews.View[]
function mod.views(entity)
   local node = TransportBelts.Node.create(entity)
   local analysis = node:belt_analyzer_algo()
   local left, right = analysis.left, analysis.right
   return {
      slots_view(entity, node),
      totals_view(entity, { "fa.ui-belt-analyzer-tab-total" }, { left.total, right.total }, analysis.total_length),
      totals_view(
         entity,
         { "fa.ui-belt-analyzer-tab-upstream" },
         { left.upstream, right.upstream },
         analysis.upstream_length
      ),
      totals_view(
         entity,
         { "fa.ui-belt-analyzer-tab-downstream" },
         { left.downstream, right.downstream },
         analysis.downstream_length
      ),
   }
end

EntityViews.register({ "transport-belt" }, defines.relative_gui_type.transport_belt_gui, mod.views)

return mod
