--Here: Functions for building with the mod, both basics and advanced tools.
local BotLogistics = require("scripts.worker-robots")
local BuildDimensions = require("scripts.build-dimensions")
local Electrical = require("scripts.electrical")
local FaUtils = require("scripts.fa-utils")
local Fluids = require("scripts.fluids")
local Localising = require("scripts.localising")
local NativeCursor = require("scripts.native-cursor")
local dirs = defines.direction
local Speech = require("scripts.speech")
local MessageBuilder = Speech.MessageBuilder
local TransportBelts = require("scripts.transport-belts")
local Viewpoint = require("scripts.viewpoint")

local mod = {}

---Maximum connection radius of any roboport prototype.
---Uses logistic_radius as approximation since logistics_connection_distance >= logistic_radius.
local max_roboport_connection_radius = 0
do
   local roboports = prototypes.get_entity_filtered({ { filter = "type", type = "roboport" } })
   for _, proto in pairs(roboports) do
      if proto.logistic_radius and proto.logistic_radius > max_roboport_connection_radius then
         max_roboport_connection_radius = proto.logistic_radius
      end
   end
   -- Fallback to vanilla value if no roboports found (shouldn't happen)
   if max_roboport_connection_radius == 0 then max_roboport_connection_radius = 25 end
end

---Check if the item in cursor can be rotated (has rotation support)
---@param stack LuaItemStack
---@return boolean can_rotate True if the stack can be rotated
function mod.can_rotate_item(stack)
   if not stack or not stack.valid_for_read or not stack.valid then return false end

   local rotation_count = BuildDimensions.get_rotation_count(stack)
   return rotation_count ~= nil
end

---@class fa.BuildingTools.HeldConnection
---@field kind "pipe"|"underground"|"heat"
---@field side defines.direction The side of the footprint it leaves from
---@field tile integer Counted from the west along north and south sides, from the north along east and west

---Turns an offset from the centre and a direction a quarter clockwise `turns` times.
---@return number, number, defines.direction
local function turn_clockwise(x, y, direction, turns)
   for _ = 1, turns do
      x, y, direction = -y, x, (direction + dirs.east) % 16
   end
   return x, y, direction
end

---A mirrored entity is flipped across its own north-south line before it turns, which on the map
---is the line it faces along.
---@return number, number, defines.direction
local function mirror(x, y, direction, facing)
   if facing % 8 == 0 then return -x, y, (16 - direction) % 16 end
   return x, -y, (24 - direction) % 16
end

---@param connections fa.BuildingTools.HeldConnection[]
---@param held fa.NativeHeldBuild
---@param kind "pipe"|"underground"|"heat"
---@param x number Offset of the connecting tile from the centre, as held but before mirroring
---@param y number
---@param direction defines.direction
local function add_connection(connections, held, kind, x, y, direction)
   if held.mirrored then
      x, y, direction = mirror(x, y, direction, held.direction)
   end
   local along = (direction == dirs.north or direction == dirs.south) and x + held.width / 2 or y + held.height / 2
   table.insert(connections, { kind = kind, side = direction, tile = math.floor(along) + 1 })
end

---Where the pipe and heat connections of the entity in hand leave its footprint, built as held.
---@param prototype LuaEntityPrototype
---@param held fa.NativeHeldBuild
---@return fa.BuildingTools.HeldConnection[]
local function held_connections(prototype, held)
   local connections = {}
   if held.direction % 4 ~= 0 then return connections end
   local turns = held.direction / 4
   for _, fluidbox in ipairs(prototype.fluidbox_prototypes) do
      for _, connection in ipairs(fluidbox.pipe_connections) do
         local kind = connection.connection_type == "normal" and "pipe"
            or connection.connection_type == "underground" and "underground"
         if kind then
            -- positions holds the connecting tile for each of the four directions the entity faces
            local position = connection.positions[turns + 1]
            local _, _, direction = turn_clockwise(0, 0, connection.direction, turns)
            add_connection(connections, held, kind, position.x, position.y, direction)
         end
      end
   end
   local heat = prototype.heat_buffer_prototype or prototype.heat_energy_source_prototype
   if heat then
      for _, connection in ipairs(heat.connections) do
         -- The game gives a Vector as an array
         local x, y, direction =
            turn_clockwise(connection.position[1], connection.position[2], connection.direction, turns)
         add_connection(connections, held, "heat", x, y, direction)
      end
   end
   return connections
end

local CONNECTION_KINDS = {
   { kind = "pipe", label = { "fa.held-build-connection-pipe" } },
   { kind = "underground", label = { "fa.held-build-connection-underground" } },
   { kind = "heat", label = { "fa.held-build-connection-heat" } },
}

---Reads the entity or blueprint in hand as the game builds it: direction, size, mirroring or flips,
---and for an entity where its pipe and heat connections leave the footprint.
---@param pindex integer
---@param stack LuaItemStack
function mod.read_held_build(pindex, stack)
   local held = NativeCursor.held_build(pindex)
   if not held then return end
   local message = MessageBuilder.new()
   message:list_item(FaUtils.direction_lookup(held.direction))
   if held.width ~= 1 or held.height ~= 1 then message:list_item({ "fa.held-build-size", held.width, held.height }) end
   if held.blueprint then
      if held.flip_horizontal then message:list_item({ "fa.held-build-flipped-horizontal" }) end
      if held.flip_vertical then message:list_item({ "fa.held-build-flipped-vertical" }) end
   else
      if held.mirrored then message:list_item({ "fa.held-build-mirrored" }) end
      local connections = held_connections(stack.prototype.place_result, held)
      for _, kind in ipairs(CONNECTION_KINDS) do
         local first = true
         for _, connection in ipairs(connections) do
            if connection.kind == kind.kind then
               message:list_item()
               if first then message:fragment(kind.label) end
               first = false
               local side_length = (connection.side == dirs.north or connection.side == dirs.south) and held.width
                  or held.height
               if side_length > 1 then
                  message:fragment({
                     "fa.held-build-connection",
                     FaUtils.direction_lookup(connection.side),
                     connection.tile,
                  })
               else
                  message:fragment(FaUtils.direction_lookup(connection.side))
               end
            end
         end
      end
   end
   Speech.speak(pindex, message:build())
end

--Reads the item in hand as the game turned it
function mod.rotate_item_in_hand(event)
   local pindex = event.player_index
   local p = game.get_player(pindex)
   if not check_for_player(pindex) then return end

   local stack = p.cursor_stack

   -- Check if item in hand can rotate
   if mod.can_rotate_item(stack) then
      local drag = NativeCursor.drag_build(pindex)
      if drag and drag.turn_pending then
         Speech.speak(pindex, { "fa.drag-turn-pending" })
      else
         mod.read_held_build(pindex, stack)
      end
      return
   elseif stack and stack.valid_for_read and stack.valid and stack.prototype.place_result then
      Speech.speak(
         pindex,
         { "fa.building-no-rotate-support", Localising.get_localised_name_with_fallback(stack.prototype) }
      )
      return
   end

   -- Nothing in hand - entity rotation will be handled by on_player_rotated_entity event
end

-- The drag turn count each player last heard, while dragging. Outside storage: only this client
-- knows its drag building.
---@type table<integer, integer>
local heard_drag_turns = {}

---Called every tick: says the direction a belt drag turns to when the game makes the turn, at the
---step after rotate.
---@param pindex integer
function mod.speak_drag_turn(pindex)
   local drag = NativeCursor.drag_build(pindex)
   if not drag then
      heard_drag_turns[pindex] = nil
      return
   end
   local heard = heard_drag_turns[pindex]
   heard_drag_turns[pindex] = drag.turns
   if not heard or heard == drag.turns then return end
   local direction = NativeCursor.build_direction(pindex)
   if direction then Speech.speak(pindex, { "fa.drag-turned", { "fa.direction", direction } }) end
end

--Reads the result of rotating an entity on the map (called from on_player_rotated_entity event)
function mod.on_entity_rotated(event)
   local pindex = event.player_index
   local ent = event.entity

   if not ent or not ent.valid then return end

   -- Use direction for entities that support it, orientation for vehicles and others
   local dir
   if ent.supports_direction then
      dir = ent.direction
   else
      dir = FaUtils.get_heading_value(ent)
   end

   Speech.speak(pindex, FaUtils.direction_lookup(dir))
end

--Reads the item in hand as the game flipped it. The game's own flip control shares the key and runs
--first; for what cannot be flipped it says so itself. With nothing in hand, the
--on_player_flipped_entity event reads the entity flipped on the map.
function mod.flip_item_in_hand(event)
   local pindex = event.player_index
   local p = game.get_player(pindex)
   if not check_for_player(pindex) then return end

   local stack = p.cursor_stack
   if not (stack and stack.valid_for_read) then return end
   local held = NativeCursor.held_build(pindex)
   if held and (held.blueprint or held.flippable) then mod.read_held_build(pindex, stack) end
end

--Reads the result of flipping an entity on the map (called from on_player_flipped_entity event)
function mod.on_entity_flipped(event)
   local pindex = event.player_index
   local ent = event.entity
   local horizontal = event.horizontal

   if not ent or not ent.valid then return end

   -- Announce which flip was performed
   if horizontal then
      Speech.speak(pindex, { "fa.flipped-horizontal" })
   else
      Speech.speak(pindex, { "fa.flipped-vertical" })
   end
end

--Returns a list of positions for this entity where it has its heat pipe connections.
function mod.get_heat_connection_positions(ent_name, ent_position, ent_direction)
   local pos = ent_position
   local positions = {}
   if ent_name == "heat-pipe" then
      table.insert(positions, { x = pos.x, y = pos.y })
   elseif ent_name == "heat-exchanger" then
      table.insert(positions, FaUtils.offset_position_legacy(pos, FaUtils.rotate_180(ent_direction), 0.5))
   elseif ent_name == "nuclear-reactor" then
      table.insert(positions, { x = pos.x - 2, y = pos.y - 2 })
      table.insert(positions, { x = pos.x - 0, y = pos.y - 2 })
      table.insert(positions, { x = pos.x + 2, y = pos.y - 2 })

      table.insert(positions, { x = pos.x - 2, y = pos.y - 0 })
      table.insert(positions, { x = pos.x + 2, y = pos.y - 0 })

      table.insert(positions, { x = pos.x - 2, y = pos.y + 2 })
      table.insert(positions, { x = pos.x - 0, y = pos.y + 2 })
      table.insert(positions, { x = pos.x + 2, y = pos.y + 2 })
   end
   return positions
end

--Returns a list of positions for this entity where it expects to find other heat pipe interfaces that it can connect to.
function mod.get_heat_connection_target_positions(ent_name, ent_position, ent_direction)
   local pos = ent_position
   local positions = {}
   if ent_name == "heat-pipe" then
      table.insert(positions, { x = pos.x - 1, y = pos.y - 0 })
      table.insert(positions, { x = pos.x + 1, y = pos.y - 0 })
      table.insert(positions, { x = pos.x - 0, y = pos.y - 1 })
      table.insert(positions, { x = pos.x - 0, y = pos.y + 1 })
   elseif ent_name == "heat-exchanger" then
      table.insert(positions, FaUtils.offset_position_legacy(pos, FaUtils.rotate_180(ent_direction), 1.5))
   elseif ent_name == "nuclear-reactor" then
      table.insert(positions, { x = pos.x - 2, y = pos.y - 3 })
      table.insert(positions, { x = pos.x - 0, y = pos.y - 3 })
      table.insert(positions, { x = pos.x + 2, y = pos.y - 3 })

      table.insert(positions, { x = pos.x - 3, y = pos.y - 2 })
      table.insert(positions, { x = pos.x + 3, y = pos.y - 2 })

      table.insert(positions, { x = pos.x - 3, y = pos.y - 0 })
      table.insert(positions, { x = pos.x + 3, y = pos.y - 0 })

      table.insert(positions, { x = pos.x - 3, y = pos.y + 2 })
      table.insert(positions, { x = pos.x + 3, y = pos.y + 2 })

      table.insert(positions, { x = pos.x - 2, y = pos.y + 3 })
      table.insert(positions, { x = pos.x - 0, y = pos.y + 3 })
      table.insert(positions, { x = pos.x + 2, y = pos.y + 3 })
   end
   return positions
end

--Returns an info string about trying to build the entity in hand. The info type depends on the entity. Note: Limited usefulness for entities with sizes greater than 1 by 1.
---@param stack LuaItemStack
function mod.build_preview_checks_info(stack, pindex)
   local p = game.get_player(pindex)
   local surf = game.get_player(pindex).surface
   local vp = Viewpoint.get_viewpoint(pindex)
   local pos = vp:get_cursor_pos()
   local tile_center = FaUtils.center_of_tile(pos)

   local result = { "" }
   -- Only the player's own client knows the direction, and only that client speaks the result
   local build_dir = NativeCursor.build_direction(pindex)
   if not build_dir then return result end
   local ent_p = stack.prototype.place_result --it is an entity prototype!
   if ent_p == nil or not ent_p.valid then return "invalid entity" end

   -- Whether it can be built here, and reach, the DLL reads from the game's own preview
   --For underground belts, state the potential neighbor
   if ent_p.type == "underground-belt" then
      local entrance, actual_dist = TransportBelts.find_underground_entrance(surf, ent_p, tile_center, build_dir)
      if entrance then
         table.insert(result, {
            "fa.connection-connects-underground",
            { "fa.direction", build_dir },
            tostring(actual_dist - 1),
         })
      else
         table.insert(result, { "fa.connection-not-connected-pipe" })
      end
   end

   --For pipes to ground, state when connected
   if ent_p.type == "pipe-to-ground" then
      local connection = Fluids.get_pipe_to_ground_preview(p.surface, pos, build_dir, ent_p)
      if connection then
         table.insert(result, {
            "fa.connection-connects-underground",
            { "fa.direction", connection.facing_direction },
            tostring(connection.distance),
         })
      end
   end

   --For heat pipes, preview the connection directions
   if ent_p.type == "heat-pipe" then
      table.insert(result, { "fa.connection-heat-pipe-can-connect" })
      local con_targets = mod.get_heat_connection_target_positions("heat-pipe", pos, dirs.north)
      local con_count = 0
      if #con_targets > 0 then
         for i, con_target_pos in ipairs(con_targets) do
            --For each heat connection target position
            rendering.draw_circle({
               color = { 1.0, 0.0, 0.5 },
               radius = 0.1,
               width = 2,
               target = con_target_pos,
               surface = p.surface,
               time_to_live = 30,
            })
            local target_ents = p.surface.find_entities_filtered({ position = con_target_pos })
            for j, target_ent in ipairs(target_ents) do
               if
                  target_ent.valid
                  and #mod.get_heat_connection_positions(target_ent.name, target_ent.position, target_ent.direction)
                     > 0
               then
                  for k, spot in
                     ipairs(
                        mod.get_heat_connection_positions(target_ent.name, target_ent.position, target_ent.direction)
                     )
                  do
                     --For each heat connection of the found target entity
                     rendering.draw_circle({
                        color = { 1.0, 1.0, 0.5 },
                        radius = 0.2,
                        width = 2,
                        target = spot,
                        surface = p.surface,
                        time_to_live = 30,
                     })
                     if util.distance(con_target_pos, spot) < 0.2 then
                        --For each match
                        rendering.draw_circle({
                           color = { 0.5, 1.0, 0.5 },
                           radius = 0.3,
                           width = 2,
                           target = spot,
                           surface = p.surface,
                           time_to_live = 30,
                        })
                        con_count = con_count + 1
                        local con_dir = FaUtils.get_direction_biased(con_target_pos, pos)
                        if con_count > 1 then table.insert(result, { "fa.connection-heat-pipe-and" }) end
                        table.insert(result, { "fa.direction", con_dir })
                     end
                  end
               end
            end
         end
      end
      if con_count == 0 then table.insert(result, { "fa.connection-heat-pipe-to-nothing" }) end
   end
   --For electric poles, report the directions of up to 5 wire-connectible
   -- electric poles that can connect.
   if ent_p.type == "electric-pole" then
      -- Calculate center position of the entity being placed
      local footprint = FaUtils.calculate_building_footprint({
         entity_prototype = ent_p,
         position = pos,
         building_direction = build_dir,
      })
      local pole_center = footprint.center

      local pole_dict = surf.find_entities_filtered({
         type = "electric-pole",
         position = pole_center,
         radius = ent_p.get_max_wire_distance(stack.quality),
      })
      local poles = {}
      for i, v in pairs(pole_dict) do
         local ent_p_mwd = ent_p.get_max_wire_distance(stack.quality)
         local pole_mwd = v.prototype.get_max_wire_distance(v.quality)
         local max_allowed = math.min(ent_p_mwd, pole_mwd)
         -- Use center-to-center distance for accurate calculation
         if max_allowed >= util.distance(v.position, pole_center) then table.insert(poles, v) end
      end
      if #poles > 0 then
         --List the first 4 poles within range
         table.insert(result, { "fa.connection-connecting" })
         for i, pole in ipairs(poles) do
            if i < 5 then
               -- Use center-to-center distance for accurate reporting
               local dist = math.ceil(util.distance(pole.position, pole_center))
               local dir = FaUtils.get_direction_biased(pole.position, pole_center)
               table.insert(result, FaUtils.format_distance_with_direction(dist, helpers.direction_to_string(dir)))
               table.insert(result, ", ")
            end
         end
      else
         --Notify if no connections and state nearest electric pole
         table.insert(result, { "fa.connection-not-connected" })
         local nearest_pole, min_dist = Electrical.find_nearest_electric_pole(nil, false, 50, surf, pole_center)
         if min_dist == nil or min_dist >= 1000 then
            table.insert(result, { "fa.connection-no-poles-within" })
         else
            local dir = FaUtils.get_direction_biased(nearest_pole.position, pole_center)
            table.insert(result, {
               "fa.connection-to-nearest-pole",
               FaUtils.format_distance_with_direction(math.ceil(min_dist), helpers.direction_to_string(dir)),
            })
         end
      end
   end

   --For roboports, list possible neighbors that would connect (boxes overlap)
   if ent_p.type == "roboport" then
      -- Calculate center of roboport being placed
      local footprint = FaUtils.calculate_building_footprint({
         entity_prototype = ent_p,
         position = pos,
         building_direction = build_dir,
      })
      local new_center = footprint.center
      local new_radius = ent_p.logistic_radius -- connection distance >= logistic_radius

      -- Search box: sum of max possible connection radii for any two roboports
      local search_radius = new_radius + max_roboport_connection_radius
      local port_dict = surf.find_entities_filtered({
         type = "roboport",
         area = {
            { new_center.x - search_radius, new_center.y - search_radius },
            { new_center.x + search_radius, new_center.y + search_radius },
         },
      })

      -- Filter to only roboports that would actually connect (box overlap)
      local connecting_ports = {}
      for _, port in pairs(port_dict) do
         local port_radius = port.logistic_cell.logistics_connection_distance
         local sum_radii = new_radius + port_radius
         local dx = math.abs(new_center.x - port.position.x)
         local dy = math.abs(new_center.y - port.position.y)
         -- Boxes connect if both dx and dy are within sum of radii
         if dx <= sum_radii and dy <= sum_radii then table.insert(connecting_ports, port) end
      end

      if #connecting_ports > 0 then
         table.insert(result, { "fa.connection-connecting" })
         for i, port in ipairs(connecting_ports) do
            if i <= 5 then
               local dist = math.ceil(util.distance(port.position, new_center))
               local dir = FaUtils.get_direction_biased(port.position, new_center)
               table.insert(result, FaUtils.format_distance_with_direction(dist, helpers.direction_to_string(dir)))
               table.insert(result, ", ")
            end
         end
      else
         --Notify if no connections and state nearest roboport
         table.insert(result, { "fa.connection-not-connected" })
         -- Search for nearest roboport (any type) within a large radius
         local far_ports = surf.find_entities_filtered({
            type = "roboport",
            position = new_center,
            radius = 2000,
         })
         if #far_ports == 0 then
            table.insert(result, { "fa.connection-no-roboports-within" })
         else
            -- Find the closest one
            local nearest_port = nil
            local min_dist = math.huge
            for _, port in pairs(far_ports) do
               local dist = util.distance(port.position, new_center)
               if dist < min_dist then
                  min_dist = dist
                  nearest_port = port
               end
            end
            local dir = FaUtils.get_direction_biased(nearest_port.position, new_center)
            table.insert(result, {
               "fa.connection-to-nearest-roboport",
               FaUtils.format_distance_with_direction(math.ceil(min_dist), helpers.direction_to_string(dir)),
            })
         end
      end
   end

   --For logistic chests, list whether there is a network nearby
   if ent_p.type == "logistic-container" then
      local network, distance, direction = BotLogistics.find_closest_network_with_distance(p.surface, pos, p.force)
      if not network then
         table.insert(result, { "fa.connection-not-in-network" })
      elseif distance and distance > 0 then
         local dir = FaUtils.direction_lookup(direction)
         table.insert(
            result,
            { "fa.connection-not-in-network-nearest", FaUtils.format_distance_with_direction(math.ceil(distance), dir) }
         )
      else
         local network_name = BotLogistics.get_network_name_from_network(network)
         table.insert(result, { "fa.connection-in-network", network_name })
      end
   end

   --For all electric powered entities, note whether powered, and from which direction. Otherwise report the nearest power pole.
   if ent_p.electric_energy_source_prototype ~= nil then
      local position = pos

      position.x = position.x + math.ceil(2 * ent_p.selection_box.right_bottom.x) / 2 - 0.5
      position.y = position.y + math.ceil(2 * ent_p.selection_box.right_bottom.y) / 2 - 0.5

      local dict = prototypes.get_entity_filtered({ { filter = "type", type = "electric-pole" } })
      local poles = {}
      for i, v in pairs(dict) do
         table.insert(poles, v)
      end
      table.sort(poles, function(k1, k2)
         return k1.get_supply_area_distance() < k2.get_supply_area_distance()
      end)
      local check = false
      ---@type LuaEntity
      local found_pole = nil
      for i, pole in ipairs(poles) do
         local names = {}
         for i1 = i, #poles, 1 do
            table.insert(names, poles[i1].name)
         end
         local supply_dist = pole.get_supply_area_distance()
         if supply_dist > 15 then supply_dist = supply_dist - 2 end
         local area = {
            left_top = {
               (position.x + math.ceil(ent_p.selection_box.left_top.x) - supply_dist),
               (position.y + math.ceil(ent_p.selection_box.left_top.y) - supply_dist),
            },
            right_bottom = {
               (position.x + math.floor(ent_p.selection_box.right_bottom.x) + supply_dist),
               (position.y + math.floor(ent_p.selection_box.right_bottom.y) + supply_dist),
            },
            orientation = build_dir / (2 * dirs.south),
         } --**laterdo "connected" check is a little buggy at the supply area edges, need to trim and tune, maybe re-enable direction based offset? The offset could be due to the pole width: 1 vs 2, maybe just make it more conservative?
         local T = {
            area = area,
            name = names,
         }
         local supplier_poles = surf.find_entities_filtered(T)
         if #supplier_poles > 0 then
            check = true
            found_pole = supplier_poles[1]
            break
         end
      end
      if check then
         table.insert(result, { "fa.connection-power-connected" })
         if found_pole.valid then
            local dist = math.ceil(util.distance(found_pole.position, pos))
            local dir = FaUtils.get_direction_biased(found_pole.position, pos)
            table.insert(result, {
               "fa.connection-from-direction",
               FaUtils.format_distance_with_direction(dist, helpers.direction_to_string(dir)),
            })
         end
      else
         table.insert(result, { "fa.connection-power-not-connected" })
         --Notify if no connections and state nearest electric pole
         local nearest_pole, min_dist = Electrical.find_nearest_electric_pole(nil, false, 50, surf, pos)
         if min_dist == nil or min_dist >= 1000 then
            table.insert(result, { "fa.connection-no-poles-within" })
         else
            local dir = FaUtils.get_direction_biased(nearest_pole.position, pos)
            table.insert(result, {
               "fa.connection-to-nearest-pole",
               FaUtils.format_distance_with_direction(math.ceil(min_dist), helpers.direction_to_string(dir)),
            })
         end
      end
   end

   return result
end

--Identifies if a pipe is a pipe end, so that it can be singled out. The motivation is that pipe ends generally should not exist because the pipes should connect to something.
---@param ent LuaEntity
function mod.is_a_pipe_end(ent)
   local connections = 0
   for i = 1, ent.fluids_count do
      local outgoing = ent.get_fluid_box_pipe_connections(i)
      for j = 1, #outgoing do
         if outgoing[j].target then connections = connections + 1 end
         if connections > 1 then return false end
      end
   end

   return connections == 1
end

return mod
