--[[
Readouts the mod builds in Lua: a pipe's shape for the fluid window's own panel, and the summary
of an area. What an entity is and how it stands is said by
the DLL's tile reader (native/src/reader.h), for the cursor, the scanner and pins alike.
]]
local serpent = require("serpent")
local FaUtils = require("scripts.fa-utils")
local Fluids = require("scripts.fluids")
local Localising = require("scripts.localising")
local Speech = require("scripts.speech")
local TileKinds = require("scripts.tile-kinds")
local MessageBuilder = Speech.MessageBuilder
local NetworkShape = require("scripts.network-shape")

local mod = {}

-- The shape the connections of a pipe make ("corner north and east"), counting whatever it joins:
-- pipes, machines, tanks.
---@param ent LuaEntity a pipe or an infinity pipe
---@return LocalisedString?
function mod.pipe_shape(ent)
   local shape_info = Fluids.get_pipe_shape(ent)
   local s, d = shape_info.shape, shape_info.direction
   local conns = assert(ent.get_fluid_box_pipe_connections(1))
   local pipe_conn_count = 0
   for _, c in pairs(conns) do
      if c.target then pipe_conn_count = pipe_conn_count + 1 end
   end

   -- This is just a boring if table.  no special logic here.
   if s == NetworkShape.SHAPE.END and pipe_conn_count == 1 then
      return { "fa.ent-info-pipe-end", FaUtils.direction_lookup(FaUtils.rotate_180(d)) }
   elseif s == NetworkShape.SHAPE.ALONE and pipe_conn_count == 0 then
      return { "fa.ent-info-pipe-alone" }
   elseif s == NetworkShape.SHAPE.STRAIGHT and pipe_conn_count == 2 then
      return { d == defines.direction.north and "fa.ent-info-pipe-vertical" or "fa.ent-info-pipe-horizontal" }
   elseif s == NetworkShape.SHAPE.CORNER and pipe_conn_count == 2 then
      local c1, c2
      if d == defines.direction.northwest then
         c1 = defines.direction.south
         c2 = defines.direction.east
      elseif d == defines.direction.northeast then
         c1 = defines.direction.south
         c2 = defines.direction.west
      elseif d == defines.direction.southwest then
         c1 = defines.direction.north
         c2 = defines.direction.east
      elseif d == defines.direction.southeast then
         c1 = defines.direction.north
         c2 = defines.direction.west
      else
         error("unreachable! " .. serpent.line({ s = s, d = d }))
      end

      return { "fa.ent-info-pipe-corner", FaUtils.direction_lookup(c1), FaUtils.direction_lookup(c2) }
   elseif s == NetworkShape.SHAPE.CROSS and pipe_conn_count == 4 then
      return { "fa.ent-info-pipe-cross" }
   elseif s == NetworkShape.SHAPE.T then
      local key = "fa.ent-info-pipe-t-vertical"
      if d == defines.direction.north or d == defines.direction.south then key = "fa.ent-info-pipe-t-horizontal" end
      return { key, FaUtils.direction_lookup(FaUtils.rotate_180(d)) }
   end
end

--Returns an info string about the entities and tiles found within an area scan done by an enlarged cursor.
---@param pindex integer
---@param left_top fa.Point
---@param right_bottom fa.Point
---@param skip_types table<string, true>? Entity types to leave out of the count
---@return LocalisedString
function mod.area_scan_summary_info(pindex, left_top, right_bottom, skip_types)
   local msg = MessageBuilder.new()

   local chunk_lt_x = math.floor(left_top.x / 32)
   local chunk_lt_y = math.floor(left_top.y / 32)
   -- right_bottom is exclusive
   local chunk_rb_x = math.ceil(right_bottom.x / 32) - 1
   local chunk_rb_y = math.ceil(right_bottom.y / 32) - 1

   local player = assert(game.get_player(pindex))
   ---@cast player LuaPlayer
   local surf = player.surface

   local charted_chunk_count = 0
   local total_chunks_covered = 0
   for cx = chunk_lt_x, chunk_rb_x do
      for cy = chunk_lt_y, chunk_rb_y do
         if player.force.is_chunk_charted(surf, { cx, cy }) then charted_chunk_count = charted_chunk_count + 1 end
         total_chunks_covered = total_chunks_covered + 1
      end
   end
   if total_chunks_covered > 0 and charted_chunk_count < 1 then
      return { "fa.area-scan-charted-zero" }
   elseif total_chunks_covered > 0 and charted_chunk_count < total_chunks_covered then
      msg:fragment({
         "fa.area-scan-charted-percent",
         tostring(math.floor(charted_chunk_count / total_chunks_covered * 100)),
      })
   end

   ---@type { name: string, count: integer, category: string, tile: true? }[]
   local counts = {}

   local covered_area = (right_bottom.x - left_top.x) * (right_bottom.y - left_top.y)
   assert(covered_area > 0)

   -- Water, lava, the oceans and marshes, each by the game's name of its tile
   ---@type table<string, integer>
   local liquids = {}
   for _, tile in pairs(surf.find_tiles_filtered({ name = TileKinds.water_like(), area = { left_top, right_bottom } })) do
      liquids[tile.name] = (liquids[tile.name] or 0) + 1
   end
   for name, count in pairs(liquids) do
      table.insert(counts, { name = name, count = count, category = "resource", tile = true })
   end

   local path_count = surf.count_tiles_filtered({ name = "stone-path", area = { left_top, right_bottom } })
   if path_count > 0 then
      table.insert(counts, { name = "stone-brick-path", count = path_count, category = "flooring" })
   end

   local concrete_count = surf.count_tiles_filtered({
      name = { "concrete", "hazard-concrete-left", "hazard-concrete-right" },
      area = { left_top, right_bottom },
   })
   if concrete_count > 0 then
      table.insert(counts, { name = "concrete", count = concrete_count, category = "flooring" })
   end

   local refined_concrete_count = surf.count_tiles_filtered({
      name = { "refined-concrete", "refined-hazard-concrete-left", "refined-hazard-concrete-right" },
      area = { left_top, right_bottom },
   })
   if refined_concrete_count > 0 then
      table.insert(counts, { name = "refined-concrete", count = refined_concrete_count, category = "flooring" })
   end

   for _, res_proto in pairs(prototypes.entity) do
      if res_proto.type == "resource" then
         local res_count = surf.count_entities_filtered({ name = res_proto.name, area = { left_top, right_bottom } })
         table.insert(counts, { name = res_proto.name, count = res_count, category = "resource" })
      end
   end

   local tree_count = surf.count_entities_filtered({ type = "tree", area = { left_top, right_bottom } })
   if tree_count > 0 then table.insert(counts, { name = "trees", count = tree_count, category = "resource" }) end

   local others = surf.find_entities_filtered({
      type = { "resource", "tree" },
      area = { left_top, right_bottom },
      invert = true,
   })

   local others_by_proto = {}
   for _, ent in pairs(others) do
      if ent.valid and not (skip_types and skip_types[ent.type]) then
         others_by_proto[ent.name] = (others_by_proto[ent.name] or 0) + 1
      end
   end

   for n, c in pairs(others_by_proto) do
      if c > 0 then table.insert(counts, { name = n, count = c, category = "other" }) end
   end

   table.sort(counts, function(k1, k2)
      return k1.count > k2.count
   end)

   local count_total = 0
   for _, i in pairs(counts) do
      count_total = count_total + i.count
   end

   -- Now using Speech to properly handle localisation
   local has_items = false
   for _, entry in pairs(counts) do
      if entry.count == 0 then break end

      if not has_items then
         msg:fragment({ "fa.area-scan-contains" })
         has_items = true
      end

      -- Create proper localised string for the name
      local localised_name
      if entry.tile then
         localised_name = prototypes.tile[entry.name].localised_name
      elseif entry.name == "trees" then
         localised_name = { "fa.area-scan-trees" }
      elseif entry.name == "stone-brick-path" then
         localised_name = { "tile-name.stone-path" }
      elseif entry.name == "concrete" then
         localised_name = { "tile-name.concrete" }
      elseif entry.name == "refined-concrete" then
         localised_name = { "tile-name.refined-concrete" }
      else
         -- Every other entry is counted from entities
         localised_name = Localising.get_localised_name_with_fallback(prototypes.entity[entry.name])
      end

      if entry.category == "resource" or entry.category == "flooring" then
         msg:list_item({
            "fa.area-scan-item-with-percent",
            tostring(entry.count),
            localised_name,
            tostring(math.floor(entry.count / covered_area * 100)),
         })
      else
         msg:list_item({ "fa.area-scan-item", tostring(entry.count), localised_name })
      end
   end

   if has_items then
      msg:fragment({ "fa.area-scan-total-occupied", tostring(math.floor(count_total / covered_area * 100)) })
   else
      msg:fragment({ "fa.area-scan-empty" })
   end

   return msg:build()
end

return mod
