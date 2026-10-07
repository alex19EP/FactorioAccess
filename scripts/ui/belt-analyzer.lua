--[[
What a transport belt carries, read with the game's own belt window. The game shows a belt's items
only on the map, so when the window opens this sends the native DLL four views of them, which the
belt's screen reads after the window (native/src/screens/BeltScreen.hpp): this belt's slots, and
what the whole belt, the belts feeding it and the belts it feeds carry on each lane. They are taken
as the window opens; reopening it takes them again.
]]
local EventManager = require("scripts.event-manager")
local FaUtils = require("scripts.fa-utils")
local Geometry = require("scripts.geometry")
local ItemInfo = require("scripts.item-info")
local Speech = require("scripts.speech")
local MessageBuilder = Speech.MessageBuilder
local TH = require("scripts.table-helpers")
local TransportBelts = require("scripts.transport-belts")

---@type fa.Native?
local native = rawget(_G, "fa_native")

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

---@param pindex integer
---@param entity LuaEntity
---@param node fa.TransportBelts.Node
local function send_slots(pindex, entity, node)
   local contents = node:get_all_contents()
   native.entity_view(pindex, { "fa.ui-belt-analyzer-tab-local" })
   for lane = 1, 2 do
      local cells = {}
      for slot = 1, 4 do
         cells[slot] = slot_cell(contents, lane, slot)
      end
      native.entity_view_column(pindex, lane_name(entity, lane), table.unpack(cells))
   end
end

-- Each lane's items, most first, with their share of the lane's length.
---@param pindex integer
---@param entity LuaEntity
---@param title LocalisedString
---@param lanes fa.NQC[] left lane, then right
---@param lengths fa.TransportBelts.LaneLengths
local function send_totals(pindex, entity, title, lanes, lengths)
   native.entity_view(pindex, title)
   if not next(lanes[1]) and not next(lanes[2]) then
      native.entity_view_column(pindex, "", { "fa.ui-belt-analyzer-no-contents" })
      return
   end
   for lane = 1, 2 do
      local length = lengths[lane == 1 and "left" or "right"]
      local cells = {}
      for _, entry in ipairs(TH.nqc_to_sorted_descending(lanes[lane])) do
         local percent = length > 0 and string.format("%.1f", 100 * entry.count / length) or "0.0"
         table.insert(cells, { "fa.ui-belt-analyzer-aggregation", ItemInfo.item_info(entry), percent })
      end
      if not cells[1] then cells[1] = { "fa.ui-belt-analyzer-empty" } end
      native.entity_view_column(pindex, lane_name(entity, lane), table.unpack(cells))
   end
end

---@param pindex integer
---@param entity LuaEntity a transport belt
function mod.send_views(pindex, entity)
   local node = TransportBelts.Node.create(entity)
   local analysis = node:belt_analyzer_algo()
   local left, right = analysis.left, analysis.right
   native.entity_views_begin(pindex, entity.unit_number)
   send_slots(pindex, entity, node)
   send_totals(pindex, entity, { "fa.ui-belt-analyzer-tab-total" }, { left.total, right.total }, analysis.total_length)
   send_totals(
      pindex,
      entity,
      { "fa.ui-belt-analyzer-tab-upstream" },
      { left.upstream, right.upstream },
      analysis.upstream_length
   )
   send_totals(
      pindex,
      entity,
      { "fa.ui-belt-analyzer-tab-downstream" },
      { left.downstream, right.downstream },
      analysis.downstream_length
   )
   native.entity_views_end(pindex)
end

-- Only clients running the DLL read the views, and sending them changes nothing in the game.
EventManager.on_event(
   defines.events.on_gui_opened,
   ---@param event EventData.on_gui_opened
   ---@param pindex integer
   function(event, pindex)
      local entity = event.entity
      if native and event.gui_type == defines.gui_type.entity and entity.type == "transport-belt" then
         mod.send_views(pindex, entity)
      end
   end,
   EventManager.EVENT_KIND.UI
)

return mod
