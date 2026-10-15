--[[
Constants for our mod.  Must load in the data stage as well as runtime.
]]

local mod = {}

-- Numeric limits for Factorio's 32-bit integer fields
mod.INT32_MAX = 2147483647
mod.INT32_MIN = -2147483648

-- The unit vectors of the directions in order north going clockwise.  If indexed by defines.direction, this gives back
-- the unit vector pointing in that direction.  Note that this is a 0-indexed table, 0 is north, directions go
-- clockwise, 15 is northnorthwest.
---@type table<defines.direction, fa.Point>
mod.DIRECTION_VECTORS = {
   [defines.direction.north] = { x = 0.0, y = -1.0 },
   [defines.direction.northnortheast] = { x = 0.3826834323650898, y = -0.9238795325112867 },
   [defines.direction.northeast] = { x = 0.7071067811865476, y = -0.7071067811865476 },
   [defines.direction.eastnortheast] = { x = 0.9238795325112867, y = -0.38268343236508984 },
   [defines.direction.east] = { x = 1.0, y = 0.0 },
   [defines.direction.eastsoutheast] = { x = 0.9238795325112867, y = 0.3826834323650897 },
   [defines.direction.southeast] = { x = 0.7071067811865476, y = 0.7071067811865475 },
   [defines.direction.southsoutheast] = { x = 0.3826834323650899, y = 0.9238795325112867 },
   [defines.direction.south] = { x = 0.0, y = 1.0 },
   [defines.direction.southsouthwest] = { x = -0.38268343236508967, y = 0.9238795325112868 },
   [defines.direction.southwest] = { x = -0.7071067811865475, y = 0.7071067811865477 },
   [defines.direction.westsouthwest] = { x = -0.9238795325112865, y = 0.38268343236509034 },
   [defines.direction.west] = { x = -1.0, y = 0.0 },
   [defines.direction.westnorthwest] = { x = -0.9238795325112866, y = -0.38268343236509 },
   [defines.direction.northwest] = { x = -0.7071067811865477, y = -0.7071067811865474 },
   [defines.direction.northnorthwest] = { x = -0.3826834323650904, y = -0.9238795325112865 },
}

---@type table<string, true?>
mod.CRAFTING_MACHINES = {
   ["assembling-machine"] = true,
   ["furnace"] = true,
   ["rocket-silo"] = true,
}

---@type string[]
mod.RAIL_TYPES = {
   "straight-rail",
   "curved-rail-a",
   "curved-rail-b",
   "half-diagonal-rail",
}

-- Entity types that players can walk over without collision
mod.ENT_TYPES_YOU_CAN_WALK_OVER = {
   "resource",
   "transport-belt",
   "underground-belt",
   "splitter",
   "item-entity",
   "entity-ghost",
   "heat-pipe",
   "pipe",
   "pipe-to-ground",
   "character",
   "rail-signal",
   "highlight-box",
   "combat-robot",
   "logistic-robot",
   "construction-robot",
   "rocket-silo-rocket-shadow",
}

-- Entity names to exclude from various operations
mod.EXCLUDED_ENT_NAMES = { "highlight-box" }

return mod
