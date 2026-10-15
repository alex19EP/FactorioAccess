--[[
Get data about (but do not announce directly) the fluid states of an entity.

Pipes have fluidboxes, but we treat them specially, because announcing 4 fluidboxes is entirely unhelpful.  For that we
compute corner shapes and such and provide helpful descriptions.

The logic for determining pipe shapes (corner, T, cross, etc.) is shared via `network-shape.lua`.
]]
local FaUtils = require("scripts.fa-utils")
local NetworkShape = require("scripts.network-shape")

local mod = {}

--[[
Given a pipe entity, determine on which sides it is connected (to a pipe, a machine, a tank or anything else with a
fluidbox) and pass back information on the shape.  This comes back
as two values, a kind and a direction. Shape interpretations are as follows:

- straight: a line of 3 segments and this pipe is the middle. Direction is either north or east, specifying if it is
  vertical or horizontal.
- end: has one connection. Direction is the way the end "points" e.g. 180 from the connection.
- corner: an L formed of 3 pipe segments.  The direction is the corner of the imaginary box which would be formed of 4
  corners.  For example, northwest means connecting south and east, because if you completed the box it'd be the
  northwest corner.
- Cross: direction is returned as north.  Looks like a cross, which is invariant under rotation.
- alone: nothing connects, will just pass back north.
- T: the direction is the top of the T, e.g. the missing one.

This mismatches fa-info.  The trouble is that the above directions make sense for analysis and exposing them directly
would be the least verbose option, but that relies on blind people knowing what a T is, and figuring out what "northwest
corner" means--we'd have to teach it.  We probably do at some point, but that's not now, so fa-info simplifies.

This function considers only pipe entities, and counts only what is connected now.
]]
---@param ent LuaEntity
---@return { shape: fa.NetworkShape.Shape, direction: defines.direction }
function mod.get_pipe_shape(ent)
   assert(ent.type == "pipe" or ent.type == "infinity-pipe")
   assert(ent.fluids_count == 1)

   local dirs = {}
   local conns = assert(ent.get_fluid_box_pipe_connections(1))

   for _, conn in pairs(conns) do
      if conn.target then
         local dx = conn.target_position.x - conn.position.x
         local dy = conn.target_position.y - conn.position.y
         local dir = FaUtils.direction_of_vector({ x = dx, y = dy })
         dirs[dir] = true
      end
   end

   return NetworkShape.get_shape_from_directions(dirs)
end

return mod
