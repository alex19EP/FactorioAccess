--[[
Where a pipe, a pipe to ground, a storage tank or a pump connects, and what its pipeline reaches,
beside the game's own window for it (see entity-views.lua). The window shows the fluid and how much
there is, but not where the pipes go: these views show each connection of this entity, then, but
for pumps, how far the pipeline spans and the buildings it reaches. Pumps belong to no pipeline: a
pump joins two, an offshore pump feeds one, so their windows get the connections alone.
]]
local EntityViews = require("scripts.ui.entity-views")
local FaInfo = require("scripts.fa-info")
local FaUtils = require("scripts.fa-utils")
local Localising = require("scripts.localising")
local Speech = require("scripts.speech")
local MessageBuilder = Speech.MessageBuilder

local mod = {}

-- The entities a pipeline is made of rather than ones it reaches.
local PIPES = { ["pipe"] = true, ["pipe-to-ground"] = true }

-- Fluid boxes walked at most, so a base wide pipeline does not stall the game as the window opens.
local WALK_LIMIT = 5000

---@type table<FluidFlowDirection, string>
local FLOW_INTO_ENTITY = { ["input"] = "fa.fluid-views-flow-in", ["output"] = "fa.fluid-views-flow-out" }

---@type table<FluidFlowDirection, string>
local FLOW_FROM_PIPELINE = {
   ["input"] = "fa.fluid-views-flow-takes",
   ["output"] = "fa.fluid-views-flow-feeds",
   ["input-output"] = "fa.fluid-views-flow-both",
}

-- Where the other end of an underground connection is, or nil while it has none.
---@param c PipeConnection
---@return MapPosition?
local function underground_partner(c)
   if not c.target then return nil end
   return c.target.get_fluid_box_pipe_connections(c.target_fluidbox_index)[c.target_pipe_connection_index].position
end

-- The side of the entity a connection leaves from. An underground one that has no partner yet goes
-- the way a pipe to ground faces.
---@param entity LuaEntity
---@param c PipeConnection
---@return defines.direction
local function side_of(entity, c)
   local to = c.target_position
   if c.connection_type ~= "normal" then
      to = underground_partner(c)
      if not to then return entity.direction end
   end
   return FaUtils.direction_of_vector({ x = to.x - c.position.x, y = to.y - c.position.y })
end

-- On a side longer than a tile, which end of it a connection is at: "west" on the north side.
---@param entity LuaEntity
---@param c PipeConnection
---@param side defines.direction
---@return defines.direction?
local function end_of_side(entity, c, side)
   local along_x = side == defines.direction.north or side == defines.direction.south
   local offset = along_x and c.position.x - entity.position.x or c.position.y - entity.position.y
   if math.abs(offset) < 0.5 then return nil end
   if along_x then return offset < 0 and defines.direction.west or defines.direction.east end
   return offset < 0 and defines.direction.north or defines.direction.south
end

---@param entity LuaEntity
---@param c PipeConnection
---@return LocalisedString
local function connection_cell(entity, c)
   local message = MessageBuilder.new()
   local side = side_of(entity, c)
   local at = end_of_side(entity, c, side)
   if at then
      message:list_item({ "fa.fluid-views-side-at-end", FaUtils.direction_lookup(side), FaUtils.direction_lookup(at) })
   else
      message:list_item(FaUtils.direction_lookup(side))
   end

   if c.connection_type ~= "normal" then
      local partner = underground_partner(c)
      if partner then
         local gap = math.floor(FaUtils.distance(c.position, partner) + 0.5) - 1
         message:list_item({ "fa.fluid-views-underground", gap })
      else
         message:list_item({ "fa.fluid-views-underground-not-connected" })
      end
   end
   if c.target then
      message:list_item(Localising.get_localised_name_with_fallback(c.target))
   elseif c.connection_type == "normal" then
      message:list_item({ "fa.fluid-views-not-connected" })
   end

   local flow = FLOW_INTO_ENTITY[c.flow_direction]
   if flow then message:list_item({ flow }) end
   return message:build()
end

-- The connections view: a pipe's shape, then a cell per connection.
---@param entity LuaEntity
---@return LocalisedString[]
function mod.connection_cells(entity)
   local cells = {}
   if entity.type == "pipe" then
      local shape = FaInfo.pipe_shape(entity)
      if shape then table.insert(cells, shape) end
   end
   for i = 1, entity.fluids_count do
      for _, c in ipairs(entity.get_fluid_box_pipe_connections(i)) do
         table.insert(cells, connection_cell(entity, c))
      end
   end
   if not cells[1] then cells[1] = { "fa.fluid-views-no-connections" } end
   return cells
end

---@class fa.FluidViews.Reached
---@field name string
---@field flow FluidFlowDirection
---@field count integer

-- How the pipeline meets a building it reaches: which way fluid goes through the building's
-- connection to the pipeline's entity `from`.
---@param building LuaEntity
---@param index integer the building's fluid box the pipeline joins
---@param from LuaEntity
---@return FluidFlowDirection
local function flow_with(building, index, from)
   for _, c in ipairs(building.get_fluid_box_pipe_connections(index)) do
      if c.target == from then return c.flow_direction end
   end
   return "input-output"
end

-- Every fluid box of the pipeline from `entity`'s first, through the boxes of the same fluid
-- segment, which a boiler's water passing through belongs to as much as the pipes do. Buildings
-- are counted once each, pipes and pipes to ground together.
---@param entity LuaEntity
---@return { pipes: integer, reached: fa.FluidViews.Reached[], complete: boolean }
function mod.walk_pipeline(entity)
   local segment = entity.get_fluid_segment_id(1)
   local seen_boxes = { [entity.unit_number .. ":1"] = true }
   local seen_entities = { [entity.unit_number] = true }
   local pipes = PIPES[entity.type] and 1 or 0
   ---@type table<string, fa.FluidViews.Reached>
   local reached = {}
   local queue = { { entity = entity, index = 1 } }
   local head = 1

   while queue[head] and head <= WALK_LIMIT do
      local here = queue[head]
      head = head + 1
      for _, n in ipairs(here.entity.get_fluid_box_neighbours(here.index)) do
         local key = n.entity.unit_number .. ":" .. n.index
         if not seen_boxes[key] then
            seen_boxes[key] = true
            local joined = n.entity.has_fluid_segment(n.index) and n.entity.get_fluid_segment_id(n.index) == segment
            if joined then table.insert(queue, n) end
            if not seen_entities[n.entity.unit_number] then
               seen_entities[n.entity.unit_number] = true
               if PIPES[n.entity.type] then
                  pipes = pipes + 1
               else
                  local flow = flow_with(n.entity, n.index, here.entity)
                  local group = n.entity.name .. ":" .. flow
                  reached[group] = reached[group] or { name = n.entity.name, flow = flow, count = 0 }
                  reached[group].count = reached[group].count + 1
               end
            end
         end
      end
   end

   local list = {}
   for _, r in pairs(reached) do
      table.insert(list, r)
   end
   table.sort(list, function(a, b)
      if a.count ~= b.count then return a.count > b.count end
      if a.name ~= b.name then return a.name < b.name end
      return a.flow < b.flow
   end)
   return { pipes = pipes, reached = list, complete = queue[head] == nil }
end

---@param entity LuaEntity
---@return LocalisedString[]
function mod.pipeline_cells(entity)
   local walk = mod.walk_pipeline(entity)
   local box = entity.get_fluid_segment_extent_bounding_box(1)
   local width = math.ceil(box.right_bottom.x - box.left_top.x)
   local height = math.ceil(box.right_bottom.y - box.left_top.y)

   local cells = { { "fa.fluid-views-extent", walk.pipes, width, height } }
   for _, r in ipairs(walk.reached) do
      table.insert(cells, {
         "fa.fluid-views-building",
         Localising.get_localised_name_with_fallback(prototypes.entity[r.name]),
         r.count,
         { FLOW_FROM_PIPELINE[r.flow] },
      })
   end
   if not walk.reached[1] then table.insert(cells, { "fa.fluid-views-no-buildings" }) end
   if not walk.complete then table.insert(cells, { "fa.fluid-views-incomplete" }) end
   return cells
end

---@param entity LuaEntity a pipe, a pipe to ground, a storage tank, a pump or an offshore pump
---@return fa.EntityViews.View[]
function mod.views(entity)
   local views = {
      { title = { "fa.fluid-views-connections" }, columns = { { cells = mod.connection_cells(entity) } } },
   }
   if entity.has_fluid_segment(1) then
      table.insert(
         views,
         { title = { "fa.fluid-views-pipeline" }, columns = { { cells = mod.pipeline_cells(entity) } } }
      )
   end
   return views
end

EntityViews.register({ "pipe", "pipe-to-ground", "storage-tank" }, defines.relative_gui_type.pipe_gui, mod.views)
EntityViews.register({ "pump" }, defines.relative_gui_type.pump_gui, mod.views)
EntityViews.register({ "offshore-pump" }, defines.relative_gui_type.entity_with_energy_source_gui, mod.views)

return mod
