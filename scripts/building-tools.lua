--Here: Functions for building with the mod, both basics and advanced tools.
local BuildDimensions = require("scripts.build-dimensions")
local FaUtils = require("scripts.fa-utils")
local Localising = require("scripts.localising")
local NativeCursor = require("scripts.native-cursor")
local dirs = defines.direction
local Speech = require("scripts.speech")
local MessageBuilder = Speech.MessageBuilder

local mod = {}

---The item in hand, or the item whose ghost remote view puts in hand instead. Nil for an empty hand
---and for a blueprint or book from the library.
---@param player LuaPlayer
---@return LuaItemPrototype?
local function held_item(player)
   local stack = player.cursor_stack
   if stack and stack.valid_for_read then return stack.prototype end
   local ghost = player.cursor_ghost
   return ghost and ghost.name --[[@as LuaItemPrototype]]
end

---Whether the player holds a blueprint or book from the library.
---@param player LuaPlayer
---@return boolean
local function holds_library_blueprint(player)
   local record = player.cursor_record
   return record ~= nil and (record.type == "blueprint" or record.type == "blueprint-book")
end

---@class fa.BuildingTools.HeldConnection
---@field kind "pipe"|"underground"|"heat"
---@field side defines.direction The side of the footprint it leaves from
---@field tile integer Counted from the west along north and south sides, from the north along east and west
---@field flow FluidFlowDirection? A pipe's: which way fluid may pass; nil for heat

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
---@param flow FluidFlowDirection?
local function add_connection(connections, held, kind, x, y, direction, flow)
   if held.mirrored then
      x, y, direction = mirror(x, y, direction, held.direction)
   end
   local along = (direction == dirs.north or direction == dirs.south) and x + held.width / 2 or y + held.height / 2
   table.insert(connections, { kind = kind, side = direction, tile = math.floor(along) + 1, flow = flow })
end

---The side of the footprint a point outside it lies beyond, and the tile along that side it faces,
---as a connection's: counted from the west along north and south sides, from the north along east
---and west.
---@param held fa.NativeHeldBuild
---@param x number Tiles east of the centre
---@param y number Tiles south of the centre
---@return defines.direction, integer
local function side_of(held, x, y)
   if math.abs(y) > held.height / 2 then
      return y < 0 and dirs.north or dirs.south, math.floor(x + held.width / 2) + 1
   end
   return x < 0 and dirs.west or dirs.east, math.floor(y + held.height / 2) + 1
end

---A side of the footprint, with the tile along it where the side is longer than one.
---@param held fa.NativeHeldBuild
---@param side defines.direction
---@param tile integer
---@return LocalisedString
local function side_words(held, side, tile)
   local side_length = (side == dirs.north or side == dirs.south) and held.width or held.height
   if side_length > 1 then return { "fa.held-build-connection", FaUtils.direction_lookup(side), tile } end
   return FaUtils.direction_lookup(side)
end

---Where the drill in hand drops what it mines, as the game's arrow shows it on the preview, and
---what takes the items there: on a belt, the lane they go on.
---@param message fa.MessageBuilder
---@param held fa.NativeHeldBuild
local function add_output(message, held)
   if not held.output_x then return end
   local side, tile = side_of(held, held.output_x, assert(held.output_y))
   message:list_item({ "fa.held-build-output", side_words(held, side, tile) })
   if held.output_lane then
      message:fragment({
         "fa.held-build-output-into-lane",
         assert(held.output_into),
         FaUtils.direction_lookup(held.output_lane),
      })
   elseif held.output_into then
      message:fragment({ "fa.held-build-output-into", held.output_into })
   end
end

local FLOW_WORDS = {
   input = { "fa.held-build-connection-in" },
   output = { "fa.held-build-connection-out" },
}

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
            local position = connection.positions[turns + 1] --[[@as MapPosition.struct]]
            local _, _, direction = turn_clockwise(0, 0, connection.direction, turns)
            add_connection(connections, held, kind, position.x, position.y, direction, connection.flow_direction)
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
---and for an entity where a drill drops its output and where its pipe and heat connections leave
---the footprint, with which way a one-way pipe connection lets fluid pass.
---@param pindex integer
---@param item LuaItemPrototype? The item in hand or its ghost; nil for a blueprint from the library
function mod.read_held_build(pindex, item)
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
      add_output(message, held)
      local connections = held_connections(assert(assert(item).place_result), held)
      for _, kind in ipairs(CONNECTION_KINDS) do
         local first = true
         for _, connection in ipairs(connections) do
            if connection.kind == kind.kind then
               message:list_item()
               if first then message:fragment(kind.label) end
               first = false
               local flow = connection.flow and FLOW_WORDS[connection.flow]
               if flow then message:fragment(flow) end
               message:fragment(side_words(held, connection.side, connection.tile))
            end
         end
      end
   end
   Speech.speak(pindex, message:build())
end

---Whether the player holds a rail planner, or its ghost as remote view puts in hand.
---@param player LuaPlayer
---@return boolean
local function holds_rail_planner(player)
   local item = held_item(player)
   return item ~= nil and item.type == "rail-planner"
end

---Reads what rotate did to the rail planner. Not planning, it turns the straight rail a click
---places, in eight steps. While planning, it turns the end of the route, but the game keeps that
---only in ghost mode (Shift held): otherwise it picks the end itself every frame.
---@param pindex integer
local function read_rail_turn(pindex)
   local direction = NativeCursor.build_direction(pindex)
   if not NativeCursor.rail_route(pindex) then
      if direction then Speech.speak(pindex, FaUtils.direction_lookup(direction)) end
      return
   end
   if direction then
      Speech.speak(pindex, { "fa.rail-end-direction", FaUtils.direction_lookup(direction) })
   else
      Speech.speak(pindex, { "fa.rail-turn-free" })
   end
end

--Reads the item in hand as the game turned it
function mod.rotate_item_in_hand(event)
   local pindex = event.player_index
   local p = assert(game.get_player(pindex))
   if not check_for_player(pindex) then return end

   if holds_rail_planner(p) then
      read_rail_turn(pindex)
      return
   end

   local item = held_item(p)
   if (item and BuildDimensions.get_rotation_count(item)) or holds_library_blueprint(p) then
      local drag = NativeCursor.drag_build(pindex)
      if drag and drag.turn_pending then
         Speech.speak(pindex, { "fa.drag-turn-pending" })
      else
         mod.read_held_build(pindex, item)
      end
   elseif item and item.place_result then
      Speech.speak(pindex, { "fa.building-no-rotate-support", Localising.get_localised_name_with_fallback(item) })
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
   local p = assert(game.get_player(pindex))
   if not check_for_player(pindex) then return end

   local item = held_item(p)
   if not (item or holds_library_blueprint(p)) then return end
   local held = NativeCursor.held_build(pindex)
   if held and (held.blueprint or held.flippable) then mod.read_held_build(pindex, item) end
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

return mod
