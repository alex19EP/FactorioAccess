--[[
What a tile is, by what the game says of it rather than by name, so modded tiles take part.

Water-like tiles have the water_tile collision layer without ground_tile
(data/base/prototypes/tile/tile-collision-masks.lua): water, deep water, lava, the oil and ammonia
oceans, brash ice, shallow water and Gleba's wetlands. Empty space and out-of-map have both. The
DLL reads the same rule for the cursor and the scanner (native/src/prototypes.h, tileKind).
]]
local Functools = require("scripts.functools")

local mod = {}

---Names of the water-like tiles.
---@type fun(): string[]
mod.water_like = Functools.cached(function()
   local names = {}
   for name, tile in pairs(prototypes.tile) do
      local layers = tile.collision_mask.layers
      if layers.water_tile and not layers.ground_tile then table.insert(names, name) end
   end
   return names
end)

---Names of the tiles an offshore pump pumps from: those with a fluid.
---@type fun(): string[]
mod.pumpable = Functools.cached(function()
   local names = {}
   for name, tile in pairs(prototypes.tile) do
      if tile.fluid then table.insert(names, name) end
   end
   return names
end)

return mod
