--[[
What a transport belt, a splitter or a loader carries, beside the game's own window for it (see
entity-views.lua). The game shows their items only on the map. A belt gets four views: this belt's
slots, and what the whole belt, the belts feeding it and the belts it feeds carry on each lane, a
column per lane. A splitter and a loader get one, what each of their lanes holds.
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
---@param lane integer 1 or 2, left or right, facing along the belt
---@return LocalisedString
local function lane_name(entity, lane)
   local facing = entity.direction
   local side = lane == 1 and Geometry.dir_counterclockwise_90(facing) or Geometry.dir_clockwise_90(facing)
   return { "fa.ui-belt-analyzer-lane", FaUtils.direction_lookup(side) }
end

-- Whether the entity's left lane, facing along it, lies west or north: lanes are read in that order,
-- so Left and Right between their columns follow the compass.
---@param entity LuaEntity
---@return boolean
local function left_first(entity)
   return entity.direction ~= defines.direction.south and entity.direction ~= defines.direction.west
end

-- The lanes, 1 left and 2 right, west or north first.
---@param entity LuaEntity
---@return integer[]
local function lane_order(entity)
   return left_first(entity) and { 1, 2 } or { 2, 1 }
end

-- What one slot of a lane holds, then which slot it is.
---@param contents fa.TransportBelts.SlotBucket[][]
---@param lane integer 1 or 2
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
   for _, lane in ipairs(lane_order(entity)) do
      local cells = {}
      for slot = 1, 4 do
         cells[slot] = slot_cell(contents, lane, slot)
      end
      table.insert(columns, { title = lane_name(entity, lane), cells = cells })
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
   for _, lane in ipairs(lane_order(entity)) do
      local length = lengths[lane == 1 and "left" or "right"]
      local cells = {}
      for _, entry in ipairs(TH.nqc_to_sorted_descending(lanes[lane])) do
         local percent = length > 0 and string.format("%.1f", 100 * entry.count / length) or "0.0"
         table.insert(cells, { "fa.ui-belt-analyzer-aggregation", ItemInfo.item_info(entry), percent })
      end
      if not cells[1] then cells[1] = { "fa.ui-belt-analyzer-empty" } end
      table.insert(columns, { title = lane_name(entity, lane), cells = cells })
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

-- What the entity's transport lines hold together, most first.
---@param entity LuaEntity
---@param lines integer[]
---@return LocalisedString[]
local function lane_cells(entity, lines)
   ---@type fa.NQC
   local items = {}
   for _, line in ipairs(lines) do
      for _, item in
         ipairs(entity.get_transport_line(line --[[@as defines.transport_line]]).get_contents())
      do
         local quals = items[item.name] or {}
         items[item.name] = quals
         quals[item.quality] = (quals[item.quality] or 0) + item.count
      end
   end
   local cells = {}
   for _, entry in ipairs(TH.nqc_to_sorted_descending(items)) do
      table.insert(cells, ItemInfo.item_info(entry))
   end
   if not cells[1] then cells[1] = { "fa.ui-belt-analyzer-empty" } end
   return cells
end

-- A splitter's lanes across it from its left: lines 1 and 2 are its left belt's lanes, 3 and 4 its
-- right belt's, and 5 to 8 the same lanes on its output half.
local SPLITTER_LANES = {
   { belt = 1, lane = 1, lines = { 1, 5 } },
   { belt = 1, lane = 2, lines = { 2, 6 } },
   { belt = 2, lane = 1, lines = { 3, 7 } },
   { belt = 2, lane = 2, lines = { 4, 8 } },
}

---@param entity LuaEntity a splitter
---@return fa.EntityViews.View[]
function mod.splitter_views(entity)
   local facing = entity.direction
   local function side(left)
      return FaUtils.direction_lookup(
         left and Geometry.dir_counterclockwise_90(facing) or Geometry.dir_clockwise_90(facing)
      )
   end
   local columns = {}
   for _, lane in ipairs(SPLITTER_LANES) do
      local column = {
         title = { "fa.ui-belt-analyzer-splitter-lane", side(lane.belt == 1), side(lane.lane == 1) },
         cells = lane_cells(entity, lane.lines),
      }
      table.insert(columns, left_first(entity) and #columns + 1 or 1, column)
   end
   return { { title = { "fa.ui-belt-analyzer-tab-splitter" }, columns = columns } }
end

---@param entity LuaEntity a loader
---@return fa.EntityViews.View[]
function mod.loader_views(entity)
   local columns = {}
   for _, lane in ipairs(lane_order(entity)) do
      table.insert(columns, { title = lane_name(entity, lane), cells = lane_cells(entity, { lane }) })
   end
   return { { title = { "fa.ui-belt-analyzer-tab-loader" }, columns = columns } }
end

EntityViews.register({ "transport-belt" }, defines.relative_gui_type.transport_belt_gui, mod.views)
-- Four lanes side by side: narrower columns keep them on the screen.
EntityViews.register({ "splitter" }, defines.relative_gui_type.splitter_gui, mod.splitter_views, 150)
EntityViews.register({ "loader", "loader-1x1" }, defines.relative_gui_type.loader_gui, mod.loader_views)

return mod
