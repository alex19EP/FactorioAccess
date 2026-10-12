--[[
Readouts the mod builds in Lua: a pipe's shape for the fluid window's own panel, the pollution and
the nearest damaged building at a position, an item's production statistics, and the summary of an
area. What an entity is and how it stands is said by the DLL's tile reader (native/src/reader.h),
for the cursor, the scanner and pins alike.
]]
local util = require("util")
local serpent = require("serpent")
local FaUtils = require("scripts.fa-utils")
local Fluids = require("scripts.fluids")
local Localising = require("scripts.localising")
local Speech = require("scripts.speech")
local TileKinds = require("scripts.tile-kinds")
local MessageBuilder = Speech.MessageBuilder
local NetworkShape = require("scripts.network-shape")
local Cursor = require("scripts.cursor")

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

--Reads out the relative pollution level at the input position. The categories are based on data like map view shaders, water discoloration rates. For example, in default settings trees are damaged after pollution exceeds 60 and water is discolored after 90, and the deepest shader applies after 150.
function mod.read_pollution_level_at_position(pos, pindex)
   local p = game.get_player(pindex)
   local pol = p.surface.get_pollution(pos)
   local result
   if pol <= 0.1 then
      result = { "fa.pollution-level-none" }
   elseif pol < 10 then
      result = { "fa.pollution-level-minimal" }
   elseif pol < 30 then
      result = { "fa.pollution-level-low" }
   elseif pol < 60 then
      result = { "fa.pollution-level-medium" }
   elseif pol < 100 then
      result = { "fa.pollution-level-high" }
   elseif pol < 150 then
      result = { "fa.pollution-level-very-high" }
   elseif pol < 250 then
      result = { "fa.pollution-level-extremely-high" }
   else
      result = { "fa.pollution-level-maximal" }
   end
   Speech.speak(pindex, result)
end

--Reads out the distance and direction to the nearest damaged entity within 1000 tiles.
function mod.read_nearest_damaged_ent_info(pos, pindex)
   local p = game.get_player(pindex)
   --Scan for ents of your force
   local ents = p.surface.find_entities_filtered({ position = pos, radius = 1000, force = p.force })
   --Check for entities with health
   if ents == nil or #ents == 0 then
      Speech.speak(pindex, { "fa.no-damaged-structures-in-range", 1000 })
      return
   end
   local at_least_one_has_damage = false
   local damaged_ents = {}
   for i, ent in ipairs(ents) do
      if ent.is_entity_with_health == true and ent.type ~= "character" and ent.get_health_ratio() < 1 then
         at_least_one_has_damage = true
         table.insert(damaged_ents, ent)
      end
   end
   if at_least_one_has_damage == false then
      Speech.speak(pindex, { "fa.no-damaged-structures-in-range", 1000 })
      return
   end
   --Narrow by distance
   ---@type LuaEntity?
   local closest = nil
   ---@type number
   local min_dist = 1001
   for i, ent in ipairs(damaged_ents) do
      local dist = util.distance(pos, ent.position)
      if dist < min_dist then
         min_dist = dist
         closest = ent
         if min_dist < 2 then break end
      end
   end
   if closest == nil then
      Speech.speak(pindex, { "fa.no-damaged-structures-in-range", 1000 })
      return
   else
      --Move cursor to closest
      Cursor.jump(pindex, closest.position)

      --Report the result
      min_dist = math.floor(min_dist)
      local dir = FaUtils.get_direction_biased(closest.position, pos)
      local aligned_note = ""
      if FaUtils.is_direction_aligned(closest.position, pos) then aligned_note = "aligned " end
      Speech.speak(pindex, {
         "fa.damaged-entity-found",
         Localising.get_localised_name_with_fallback(closest),
         min_dist,
         aligned_note,
         FaUtils.direction_lookup(dir),
      })
   end
end

--Report total produced and consumed in last minute, ten minutes,  hour,
--thousand hours for the selected item.  The selected item comes from the item
--in hand, the selected item in an inventory, or the crafting menu's current
--selection, in that order.  Since the latter two are disjunct, this can also be
--phrased as "in hand, otherwise examine menus".  Note that Factorio stores
--fluids and items in different places, and that the complicated branching below
--must also account for that.
--
-- Recipes may also produce items as well as fluids.  In vanilla, the example is
-- barrels.  We can't do the right thing in all cases, but in vanilla it happens
-- that the stats on barrels aren't super important and, additionally, there's a
-- separate recipe one can check for that.  Since this only outputs one entry
-- when selecting a recipe, we choose the first fluid if there is one, otherwise
-- the first item.  Ultimately for mods, we're going to need a GUI for it: there
-- are too many cases in the wild.
---@param pindex integer
---@param prototype_name string|nil Optional prototype name to look up directly
function mod.selected_item_production_stats_info(pindex, prototype_name)
   local p = game.get_player(pindex)
   local stats = p.force.get_item_production_statistics(p.surface)
   local item_stack = nil
   local recipe = nil
   local prototype = nil

   -- If a prototype name was provided, try to look it up directly
   if prototype_name then
      prototype = prototypes.item[prototype_name]
      if not prototype then
         ---@type LuaFluidPrototype?
         local fluid = prototypes.fluid[prototype_name]
         if fluid then
            prototype = fluid
            stats = p.force.get_fluid_production_statistics(p.surface)
         end
      end
      -- If provided but not found, return error immediately (don't try fallbacks)
      if not prototype then return { "fa.error-unknown-item", prototype_name } end
   end

   -- Try the cursor stack (only if no prototype_name was provided)
   if not prototype then
      item_stack = p.cursor_stack
      if item_stack and item_stack.valid_for_read then prototype = item_stack.prototype end
   end

   -- UI is now handled by the new UI system.
   if not prototype then return { "fa.error-no-selected-item" } end

   -- We need both inputs and outputs. That's the same code, with one boolean
   -- changed.
   local get_stats = function(is_input)
      local name = prototype.name
      local category = is_input and "input" or "output"
      local interval = defines.flow_precision_index
      local last_minute = stats.get_flow_count({
         name = name,
         category = category,
         precision_index = interval.one_minute,
         count = true,
      })
      local last_10_minutes = stats.get_flow_count({
         name = name,
         category = category,
         precision_index = interval.ten_minutes,
         count = true,
      })
      local last_hour =
         stats.get_flow_count({ name = name, category = category, precision_index = interval.one_hour, count = true })
      local thousand_hours = stats.get_flow_count({
         name = name,
         category = category,
         precision_index = interval.one_thousand_hours,
         count = true,
      })
      last_minute = FaUtils.simplify_large_number(last_minute)
      last_10_minutes = FaUtils.simplify_large_number(last_10_minutes)
      last_hour = FaUtils.simplify_large_number(last_hour)
      thousand_hours = FaUtils.simplify_large_number(thousand_hours)
      return last_minute, last_10_minutes, last_hour, thousand_hours
   end

   local m1_in, m10_in, h1_in, h1000_in = get_stats(true)
   local m1_out, m10_out, h1_out, h1000_out = get_stats(false)

   local message = MessageBuilder.new()
   message:fragment(Localising.get_localised_name_with_fallback(prototype))
   message:list_item({ "fa.production-stats-produced" })
   message:fragment(m1_in)
   message:fragment({ "fa.production-stats-last-minute" })
   message:list_item(m10_in)
   message:fragment({ "fa.production-stats-last-ten-min" })
   message:list_item(h1_in)
   message:fragment({ "fa.production-stats-last-hour" })
   message:list_item(h1000_in)
   message:fragment({ "fa.production-stats-last-thousand-hours" })
   message:list_item({ "fa.production-stats-consumed" })
   message:fragment(m1_out)
   message:fragment({ "fa.production-stats-last-minute" })
   message:list_item(m10_out)
   message:fragment({ "fa.production-stats-last-ten-min" })
   message:list_item(h1_out)
   message:fragment({ "fa.production-stats-last-hour" })
   message:list_item(h1000_out)
   message:fragment({ "fa.production-stats-last-thousand-hours" })
   return message:build()
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
