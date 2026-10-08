--[[
Map cells: how the cursor moves on the full map.

Zoomed out to the map, a tile is a fraction of a pixel, so the movement keys move the cursor by a
cell: a square of tiles about a thirtieth of the screen across, rounded to a power of two (8 tiles at
240 tiles across, 512 at 15360). Cells lie on a grid aligned to their size, so one is inside a chunk
or made of whole chunks. The cursor stands at the centre of its cell, and moving reads what the cell
holds, then what the map's overlays that are on show over it (map-overlays.lua).
]]
local FaInfo = require("scripts.fa-info")
local FaUtils = require("scripts.fa-utils")
local Graphics = require("scripts.graphics")
local MapOverlays = require("scripts.map-overlays")
local Speech = require("scripts.speech")
local Viewpoint = require("scripts.viewpoint")

local native = rawget(_G, "fa_native")

local mod = {}

---Moves the cursor to the next cell in `direction` and reads that cell.
---@param pindex integer
---@param direction defines.direction
---@param size integer The cell size, from Zoom.get_map_cell_size
function mod.move(pindex, direction, size)
   local vp = Viewpoint.get_viewpoint(pindex)
   local target = FaUtils.offset_position_legacy(vp:get_cursor_pos(), direction, size)
   local left = math.floor(target.x / size) * size
   local top = math.floor(target.y / size) * size
   vp:set_cursor_pos_continuous({ x = left + size / 2, y = top + size / 2 }, direction)

   local left_top = { x = left, y = top }
   local right_bottom = { x = left + size, y = top + size }
   Graphics.draw_large_cursor(left_top, right_bottom, pindex)
   local player = game.get_player(pindex)
   local message = Speech.MessageBuilder.new()
   local overlays = native and native.map_overlays(pindex)
   local skip = overlays and not overlays.worker_robots and MapOverlays.WORKER_ROBOT_TYPES or nil
   message:fragment(FaInfo.area_scan_summary_info(pindex, left_top, right_bottom, skip))
   if overlays then MapOverlays.describe(message, player, left_top, right_bottom, overlays) end
   Speech.speak(pindex, message:build())
   player.play_sound({ path = "Close-Inventory-Sound", position = player.position, volume_modifier = 0.75 })
end

return mod
