--Here: Functions for building with the mod, both basics and advanced tools.
local BuildDimensions = require("scripts.build-dimensions")
local FaUtils = require("scripts.fa-utils")
local Localising = require("scripts.localising")
local NativeCursor = require("scripts.native-cursor")
local dirs = defines.direction
local Speech = require("scripts.speech")
local MessageBuilder = Speech.MessageBuilder

local mod = {}

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
