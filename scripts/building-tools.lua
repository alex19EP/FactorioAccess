--Here: Functions for building with the mod, both basics and advanced tools.
local BotLogistics = require("scripts.worker-robots")
local BuildDimensions = require("scripts.build-dimensions")
local Consts = require("scripts.consts")
local Electrical = require("scripts.electrical")
local FaUtils = require("scripts.fa-utils")
local Fluids = require("scripts.fluids")
local Localising = require("scripts.localising")
local NativeCursor = require("scripts.native-cursor")
local dirs = defines.direction
local Graphics = require("scripts.graphics")
local Speech = require("scripts.speech")
local MessageBuilder = Speech.MessageBuilder
local PlayerMiningTools = require("scripts.player-mining-tools")
local Teleport = require("scripts.teleport")
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

---@class fa.BuildingTools.BuildDecision
---@field entity_name? string Name of entity to build (nil for tiles)
---@field tile_name? string Name of tile to build (nil for entities)
---@field position MapPosition Final build position
---@field direction defines.direction Final build direction
---@field flip_horizontal boolean
---@field flip_vertical boolean
---@field footprint_left_top MapPosition
---@field footprint_right_bottom MapPosition
---@field is_tile boolean
---@field terrain_building_size? integer For tiles only
---@field skip_reason? LocalisedString If build should be skipped

---Calculate what to build and where, without actually building it.
---This is the "decider" function that determines all build parameters.
---Includes some side effects: graphics sync, storage updates, turn_to_cursor_direction_cardinal.
---@param params fa.BuildingTools.BuildItemParams
---@return fa.BuildingTools.BuildDecision? decision nil if should skip
function mod.calculate_build_params(params)
   local pindex = params.pindex
   local building_direction = params.building_direction
   local flip_horizontal = params.flip_horizontal or false
   local flip_vertical = params.flip_vertical or false

   local p = game.get_player(pindex)
   local stack = p.cursor_stack
   local vp = Viewpoint.get_viewpoint(pindex)
   local pos = vp:get_cursor_pos()

   -- Ensure building footprint is up to date
   Graphics.sync_build_cursor_graphics(pindex)

   -- Handle entities
   if stack.prototype.place_result ~= nil then
      local ent = stack.prototype.place_result
      local placing_underground_belt = stack.prototype.place_result.type == "underground-belt"

      -- Calculate footprint using centralized function
      local footprint = FaUtils.calculate_building_footprint({
         entity_prototype = stack.prototype.place_result,
         position = pos,
         building_direction = building_direction,
      })

      turn_to_cursor_direction_cardinal(pindex)

      local position = footprint.center

      -- Store the calculated footprint for later use
      storage.players[pindex].building_footprint_left_top = footprint.left_top
      storage.players[pindex].building_footprint_right_bottom = footprint.right_bottom

      local actual_build_direction = building_direction
      if placing_underground_belt then
         -- Auto-detect if placing would form an exit
         local would_auto_exit =
            TransportBelts.would_form_underground_exit(p.surface, ent, position, building_direction)
         if would_auto_exit then
            --Flip the chute by 180 degrees
            actual_build_direction = (building_direction + dirs.south) % (2 * dirs.south)
         end
      end

      return {
         entity_name = stack.prototype.place_result.name,
         tile_name = nil,
         position = position,
         direction = actual_build_direction,
         flip_horizontal = flip_horizontal,
         flip_vertical = flip_vertical,
         footprint_left_top = footprint.left_top,
         footprint_right_bottom = footprint.right_bottom,
         is_tile = false,
      }
   elseif stack and stack.valid_for_read and stack.valid and stack.prototype.place_as_tile_result ~= nil then
      -- Tile placement
      local cursor_size = vp:get_cursor_size()
      local t_size = cursor_size * 2 + 1

      pos.x = pos.x - cursor_size
      pos.y = pos.y - cursor_size
      vp:set_cursor_pos(pos)

      return {
         entity_name = nil,
         tile_name = stack.prototype.place_as_tile_result.result.name,
         position = pos,
         direction = building_direction,
         flip_horizontal = false,
         flip_vertical = false,
         footprint_left_top = pos,
         footprint_right_bottom = { x = pos.x + t_size, y = pos.y + t_size },
         is_tile = true,
         terrain_building_size = t_size,
      }
   else
      -- Empty hand or invalid item
      return nil
   end
end

---Prepare the build area by clearing obstacles and teleporting player.
---This is shared between cursor building and ghost placement.
---@param pindex integer
---@param decision fa.BuildingTools.BuildDecision
---@param teleport_player boolean
function mod.prepare_build_area(pindex, decision, teleport_player)
   -- Clear build area obstacles
   PlayerMiningTools.clear_obstacles_in_rectangle(decision.footprint_left_top, decision.footprint_right_bottom, pindex)

   -- Teleport player out of build area (if enabled)
   if teleport_player then
      mod.teleport_player_out_of_build_area(decision.footprint_left_top, decision.footprint_right_bottom, pindex)
   end
end

---@class fa.BuildingTools.BuildItemParams
---@field pindex integer Player index
---@field building_direction defines.direction Direction to build in
---@field flip_horizontal? boolean Whether to flip the blueprint horizontally (default false)
---@field flip_vertical? boolean Whether to flip the blueprint vertically (default false)
---@field teleport_player? boolean Whether to teleport player out of build area (default true)
---@field play_error_sound? boolean Whether to play error sounds (default true)
---@field speak_errors? boolean Whether to speak error messages (default true)
---@field build_mode? defines.build_mode Build mode (normal, forced, superforced). Default: normal

--[[Attempts to build the item in hand with explicit parameters.
* Does nothing if the hand is empty or the item is not a place-able entity.
* @param params fa.BuildingTools.BuildItemParams Build parameters
* @return boolean True if build was successful
]]
-- Precondition: Caller must ensure cursor_stack is valid_for_read
function mod.build_item_in_hand_with_params(params)
   local pindex = params.pindex
   local teleport_player = params.teleport_player ~= false -- default true
   local play_error_sound = params.play_error_sound ~= false -- default true
   local speak_errors = params.speak_errors ~= false -- default true

   local p = game.get_player(pindex)
   local stack = p.cursor_stack

   -- Calculate what to build (includes graphics sync, storage updates, underground belt logic, etc)
   local decision = mod.calculate_build_params(params)
   if not decision then
      -- Empty hand or invalid item
      p.play_sound({ path = "utility/cannot_build" })
      return false
   end

   -- Prepare build area (clear obstacles, teleport player)
   mod.prepare_build_area(pindex, decision, teleport_player)

   -- Execute the build
   if decision.is_tile then
      -- Tile placement
      if
         p.can_build_from_cursor({
            position = decision.position,
            terrain_building_size = decision.terrain_building_size,
         })
      then
         p.build_from_cursor({ position = decision.position, terrain_building_size = decision.terrain_building_size })
         return true
      else
         if play_error_sound then p.play_sound({ path = "utility/cannot_build" }) end
         return false
      end
   else
      -- Entity placement
      local building = {
         position = decision.position,
         direction = decision.direction,
         build_mode = params.build_mode or defines.build_mode.normal,
         flip_horizontal = decision.flip_horizontal,
         flip_vertical = decision.flip_vertical,
      }

      if p.can_build_from_cursor(building) then
         p.build_from_cursor(building)
         return true
      else
         -- Report errors (if enabled)
         if play_error_sound then p.play_sound({ path = "utility/cannot_build" }) end

         -- Explain build error (if enabled)
         if speak_errors then
            local build_area = { decision.footprint_left_top, decision.footprint_right_bottom }
            local result = mod.identify_building_obstacle(pindex, build_area, nil)
            Speech.speak(pindex, result)
         end
         return false
      end
   end
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
      mod.read_held_build(pindex, stack)
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

--Does everything to handle the nudging feature, taking the keypress event and the nudge direction as the input. Nothing happens if an entity cannot be selected.
function mod.nudge_key(direction, event)
   local pindex = event.player_index
   local p = game.get_player(pindex)
   local ent = p.selected
   local vp = Viewpoint.get_viewpoint(pindex)
   if ent and ent.valid then
      if ent.force == game.get_player(pindex).force then
         local old_pos = ent.position
         local new_pos = FaUtils.offset_position_legacy(ent.position, direction, 1)
         local temporary_teleported = false
         local actually_teleported = false

         --Clear the new build location using centralized footprint calculation
         local footprint = FaUtils.calculate_building_footprint({
            width = ent.tile_width,
            height = ent.tile_height,
            position = FaUtils.offset_position_legacy(ent.position, direction, 1),
            building_direction = ent.direction,
         })
         local left_top = footprint.left_top
         local right_bottom = footprint.right_bottom
         PlayerMiningTools.clear_obstacles_in_rectangle(left_top, right_bottom, pindex)

         --First teleport the ent to 0,0 temporarily
         temporary_teleported = ent.teleport({ 0, 0 })
         if not temporary_teleported then
            game.get_player(pindex).play_sound({ path = "utility/cannot_build" })
            Speech.speak(pindex, { "fa.failed-to-nudge" })
            return
         end

         --Now check if the ent can be placed at its new location, and proceed or revert accordingly
         local check_name = ent.name
         if check_name == "entity-ghost" then check_name = ent.ghost_name end
         if ent.surface.can_place_entity({ name = check_name, position = new_pos, direction = ent.direction }) then
            actually_teleported = ent.teleport(new_pos)
         else
            --Cannot build in new location, so send it back
            actually_teleported = ent.teleport(old_pos)
            game.get_player(pindex).play_sound({ path = "utility/cannot_build" })

            --Explain build error
            local build_area = { left_top, right_bottom }
            local obstacle_message = mod.identify_building_obstacle(pindex, build_area, ent)
            Speech.speak(pindex, obstacle_message)
            return
         end
         if not actually_teleported then
            --Failed to teleport
            Speech.speak(pindex, { "fa.failed-to-nudge" })
            return
         else
            --Successfully teleported and so nudged
            Speech.speak(pindex, { "fa.nudged-one-direction", { "fa.direction", direction } })

            vp:set_cursor_pos(FaUtils.offset_position_legacy(vp:get_cursor_pos(), direction, 1))

            if ent.type == "electric-pole" then
               -- laterdo **bugfix when nudged electric poles have extra wire reach, cut wires
               -- if ent.clone{position = new_pos, surface = ent.surface, force = ent.force, create_build_effect_smoke = false} == true then
               -- ent.destroy{}
               -- end
            end
         end
      end

      --Update ent connections after teleporting it
      ent.update_connections()
   else
      Speech.speak(pindex, { "fa.building-nudged-nothing" })
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

--For a building with fluidboxes, returns the external fluidbox and fluid name that would connect to one of the building's own fluidboxes at a particular position, from a particular direction. Importantly, ignores fluidboxes that are positioned correctly but would not connect, such as a pipe to ground facing a perpebdicular direction.
function mod.get_relevant_fluidbox_and_fluid_name(building, pos, dir_from_pos)
   local relevant_box = nil
   local relevant_fluid_name = nil
   if building ~= nil and building.valid and building.fluids_count > 0 then
      rendering.draw_circle({
         color = { 1, 1, 0 },
         radius = 0.2,
         width = 2,
         target = building.position,
         surface = building.surface,
         time_to_live = 30,
      })
      --Run checks to see if we have any fluidboxes that are relevant
      for i = 1, building.fluids_count, 1 do
         for j, con in ipairs(building.get_fluid_box_pipe_connections(i)) do
            local target_pos = con.target_position
            local con_pos = con.position
            rendering.draw_circle({
               color = { 1, 0, 0 },
               radius = 0.2,
               width = 2,
               target = target_pos,
               surface = building.surface,
               time_to_live = 30,
            })
            if
               util.distance(target_pos, pos) < 0.3
               and FaUtils.get_direction_biased(con_pos, pos) == dir_from_pos
               and not (building.name == "pipe-to-ground" and building.direction == dir_from_pos)
            then --Note: We correctly ignore the backside of a pipe to ground.
               rendering.draw_circle({
                  color = { 0, 1, 0 },
                  radius = 0.3,
                  width = 2,
                  target = target_pos,
                  surface = building.surface,
                  time_to_live = 30,
               })
               local fluid = building.get_fluid(i)
               relevant_box = fluid
               if fluid ~= nil then
                  relevant_fluid_name = fluid.name
               else
                  local filt = building.get_fluid_filter(i)
                  if filt and filt.fluid then
                     relevant_fluid_name = type(filt.fluid) == "string" and filt.fluid or filt.fluid.name
                  else
                     relevant_fluid_name = nil -- Empty pipe, no fluid
                  end
               end
            end
         end
      end
   end
   return relevant_box, relevant_fluid_name, dir_from_pos
end

--If the player is standing within the build area, they are teleported out.
function mod.teleport_player_out_of_build_area(left_top, right_bottom, pindex)
   local p = game.get_player(pindex)
   if not p.character then return end
   if not left_top or not right_bottom then return end
   local pos = p.character.position
   if pos.x < left_top.x or pos.x > right_bottom.x or pos.y < left_top.y or pos.y > right_bottom.y then return end
   if p.walking_state.walking == true then return end

   local exits = {}
   exits[1] = { x = left_top.x - 1, y = left_top.y - 0 }
   exits[2] = { x = left_top.x - 0, y = left_top.y - 1 }
   exits[3] = { x = left_top.x - 1, y = left_top.y - 1 }
   exits[4] = { x = left_top.x - 2, y = left_top.y - 0 }
   exits[5] = { x = left_top.x - 0, y = left_top.y - 2 }
   exits[6] = { x = left_top.x - 2, y = left_top.y - 2 }

   --Teleport to exit spots if possible
   for i, pos in ipairs(exits) do
      if p.surface.can_place_entity({ name = "character", position = pos }) then
         Teleport.teleport_to_closest(pindex, pos, false, true)
         return
      end
   end

   --Teleport best effort to -2, -2
   Teleport.teleport_to_closest(pindex, exits[6], false, true)
end

--Assuming there is a steam engine in hand, this function will automatically build it next to a suitable boiler.
function mod.snap_place_steam_engine_to_a_boiler(pindex)
   local p = game.get_player(pindex)
   local found_empty_spot = false
   local found_valid_spot = false
   --Locate all boilers within 10m
   local boilers = p.surface.find_entities_filtered({ name = "boiler", position = p.position, radius = 10 })

   --If none then locate all boilers within 25m
   if boilers == nil or #boilers == 0 then
      boilers = p.surface.find_entities_filtered({ name = "boiler", position = p.position, radius = 25 })
   end

   if boilers == nil or #boilers == 0 then
      p.play_sound({ path = "utility/cannot_build" })
      Speech.speak(pindex, { "fa.building-error-no-boilers" })
      return
   end

   --For each boiler found:
   for i, boiler in ipairs(boilers) do
      --Check if there is any entity in front of it
      local output_location = FaUtils.offset_position_legacy(boiler.position, boiler.direction, 1.5)
      rendering.draw_circle({
         color = { 1, 1, 0.25 },
         radius = 0.25,
         width = 2,
         target = output_location,
         surface = p.surface,
         time_to_live = 60,
         draw_on_ground = false,
      })
      local output_ents = p.surface.find_entities_filtered({
         position = output_location,
         radius = 0.25,
         type = { "resource", "generator" },
         invert = true,
      })
      if output_ents == nil or #output_ents == 0 then
         --Determine engine position based on boiler direction
         found_empty_spot = true
         local engine_position = output_location
         local dir = boiler.direction
         if dir == dirs.east then
            engine_position = FaUtils.offset_position_legacy(engine_position, dirs.east, 2)
         elseif dir == dirs.south then
            engine_position = FaUtils.offset_position_legacy(engine_position, dirs.south, 2)
         elseif dir == dirs.west then
            engine_position = FaUtils.offset_position_legacy(engine_position, dirs.west, 2)
         elseif dir == dirs.north then
            engine_position = FaUtils.offset_position_legacy(engine_position, dirs.north, 2)
         end
         rendering.draw_circle({
            color = { 0.25, 1, 0.25 },
            radius = 0.5,
            width = 2,
            target = engine_position,
            surface = p.surface,
            time_to_live = 60,
            draw_on_ground = false,
         })
         PlayerMiningTools.clear_obstacles_in_circle(engine_position, 4, pindex)
         --Check if can build from cursor to the relative position
         if p.can_build_from_cursor({ position = engine_position, direction = dir }) then
            p.build_from_cursor({ position = engine_position, direction = dir })
            found_valid_spot = true
            Speech.speak(
               pindex,
               { "fa.building-placed-steam-engine", math.floor(boiler.position.x), math.floor(boiler.position.y) }
            )
            return
         end
      end
   end
   --If all have been skipped and none were found then play error
   if found_empty_spot == false then
      p.play_sound({ path = "utility/cannot_build" })
      Speech.speak(pindex, { "fa.building-error-boilers-blocked" })
      return
   elseif found_valid_spot == false then
      p.play_sound({ path = "utility/cannot_build" })
      Speech.speak(pindex, { "fa.building-error-boilers-obstacles" })
      return
   end
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

--Scans an area to identify obstacles for building there
function mod.identify_building_obstacle(pindex, area, ent_to_ignore)
   local p = game.get_player(pindex)
   local ent_ignored = ent_to_ignore or nil
   local message = MessageBuilder.new()
   message:fragment({ "fa.cannot-build" })

   --Check for an entity in the way
   local ents_in_area = p.surface.find_entities_filtered({
      area = area,
      invert = true,
      type = Consts.ENT_TYPES_YOU_CAN_BUILD_OVER,
   })
   local obstacle_ent = nil
   for i, area_ent in ipairs(ents_in_area) do
      if
         area_ent.valid
         and area_ent.prototype.tile_width
         and area_ent.prototype.tile_width > 0
         and area_ent.prototype.tile_height
         and area_ent.prototype.tile_height > 0
         and (ent_ignored == nil or ent_ignored.unit_number ~= area_ent.unit_number)
      then
         obstacle_ent = area_ent
      end
   end
   --Check for water in the area
   local water_tiles_in_area = p.surface.find_tiles_filtered({
      area = area,
      invert = false,
      name = {
         "water",
         "deepwater",
         "water-green",
         "deepwater-green",
         "water-shallow",
         "water-mud",
         "water-wube",
      },
   })
   --Report obstacles
   if obstacle_ent ~= nil then
      message:fragment({
         "fa.building-obstacle-in-way",
         Localising.get_localised_name_with_fallback(obstacle_ent),
         math.floor(obstacle_ent.position.x),
         math.floor(obstacle_ent.position.y),
      })
   elseif #water_tiles_in_area > 0 then
      local water = water_tiles_in_area[1]
      message:fragment({ "fa.building-water-in-way", math.floor(water.position.x), math.floor(water.position.y) })
   end
   return message:build()
end

return mod
