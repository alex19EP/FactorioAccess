--[[
Map cells: how the cursor moves on the full map.

Zoomed out to the map, a tile is a fraction of a pixel, so the movement keys move the cursor by a
cell: a square of tiles about a thirtieth of the screen across, rounded to a power of two (8 tiles at
240 tiles across, 512 at 15360). Cells lie on a grid aligned to their size, so one is inside a chunk
or made of whole chunks. The DLL moves the cursor (native/src/cursormoves.h computes the cell the
same way) to the centre of the next cell, and moving reads what the cell holds, then what the map's
overlays that are on show over it (map-overlays.lua).
]]
local Cursor = require("scripts.cursor")
local FaInfo = require("scripts.fa-info")
local MapOverlays = require("scripts.map-overlays")
local Speech = require("scripts.speech")

local native = rawget(_G, "fa_native")

local mod = {}

---Reads the cell the cursor is in. The DLL moves the cursor by a cell on the full map, to the cell's
---centre.
---@param pindex integer
---@param size integer The cell size, from Zoom.get_map_cell_size
function mod.read(pindex, size)
   local cursor = Cursor.tile(pindex)
   local left = math.floor(cursor.x / size) * size
   local top = math.floor(cursor.y / size) * size

   local left_top = { x = left, y = top }
   local right_bottom = { x = left + size, y = top + size }
   local player = assert(game.get_player(pindex))
   local message = Speech.MessageBuilder.new()
   local overlays = native and native.map_overlays(pindex)
   local skip = overlays and not overlays.worker_robots and MapOverlays.WORKER_ROBOT_TYPES or nil
   message:fragment(FaInfo.area_scan_summary_info(pindex, left_top, right_bottom, skip))
   if overlays then MapOverlays.describe(message, player, left_top, right_bottom, overlays) end
   Speech.speak(pindex, message:build())
end

return mod
