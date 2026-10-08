--[[
Map tags: the labelled markers players put on the map, which the map shows and its search finds.
They are read from the player's force when the scanner refreshes, so there is nothing to track.
]]
local CircuitNetwork = require("scripts.circuit-network")
local SC = require("scripts.scanner.scanner-consts")
local Speech = require("scripts.speech")

local mod = {}

---Speaks a tag as the map shows it: its text, then its icon.
---@param message fa.MessageBuilder
---@param tag LuaCustomChartTag
function mod.describe_tag(message, tag)
   if tag.text ~= "" then message:list_item(tag.text) end
   if tag.icon then message:list_item(CircuitNetwork.localise_signal(tag.icon)) end
end

---@class fa.scanner.ChartTagsBackend: fa.scanner.ScannerBackend
local ChartTagsBackend = {}
mod.ChartTagsBackend = ChartTagsBackend
local ChartTagsBackend_meta = { __index = ChartTagsBackend }
if script then script.register_metatable("fa.scanner.ChartTagsBackend", ChartTagsBackend_meta) end

function ChartTagsBackend.new()
   return setmetatable({}, ChartTagsBackend_meta)
end

function ChartTagsBackend:on_new_entity(e) end

function ChartTagsBackend:on_entity_destroyed(event) end

function ChartTagsBackend:on_new_tiles(tiles) end

---@param player LuaPlayer
---@param e fa.scanner.ScanEntry
function ChartTagsBackend:validate_entry(player, e)
   local tag = e.backend_data
   return tag.valid and tag.surface.index == player.surface_index
end

---A tag can be moved on the map.
---@param player LuaPlayer
---@param e fa.scanner.ScanEntry
function ChartTagsBackend:update_entry(player, e)
   e.position = e.backend_data.position
end

---@param player LuaPlayer
---@param e fa.scanner.ScanEntry
function ChartTagsBackend:readout_entry(player, e)
   local message = Speech.MessageBuilder.new()
   mod.describe_tag(message, e.backend_data)
   message:list_item({ "fa.scanner-tag" })
   return message:build()
end

---@param player LuaPlayer
---@param callback fun(fa.scanner.ScanEntry)
function ChartTagsBackend:dump_entries_to_callback(player, callback)
   for _, tag in pairs(player.force.find_chart_tags(player.surface)) do
      callback({
         position = tag.position,
         backend = self,
         backend_data = tag,
         category = SC.CATEGORIES.TAGS,
         subcategory = "tag",
      })
   end
end

function ChartTagsBackend:is_huge(e)
   return false
end

function ChartTagsBackend:get_aabb(e)
   local p = e.backend_data.position
   return p.x, p.y, p.x, p.y
end

return mod
