-- Builds the base map of the Factorio Access tutorial campaign (campaigns/factorio-access).
--
-- A level needs its own map file, blueprint.zip, or the game opens the map generator before it.
-- Only the map editor writes one, so: copy this folder to the user data scenarios folder, start the
-- map editor with it as a new scenario, type /fabase in the console, then save the scenario and copy
-- its blueprint.zip into the level. Each level's control.lua builds the rest of its layout on this
-- map.
--
-- The map: 400 by 400 tiles around 0, 0; grass in four shades with patches of sand in three, from
-- the fixed noise below rather than the game's terrain, so a rebuild gives the same map; no ore,
-- trees, rocks, decoratives, cliffs or enemies; one rectangular pond north east of the spawn.

local SIZE = 400
local HALF = 200
-- Chunks around chunk 0, 0 that cover the map: -7 to 6 each way.
local CHUNK_RADIUS = 7

-- The pond, in tiles: left top corner and size.
local POND = { x = 16, y = -14, width = 8, height = 6 }

-- Sand where the patch noise is above this: about a quarter of the map, in patches.
local SAND_LEVEL = 0.6

---A fixed pseudo random value in [0, 1) for a lattice point.
---@param ix integer
---@param iy integer
---@param seed number
---@return number
local function lattice(ix, iy, seed)
   local v = math.sin(ix * 127.1 + iy * 311.7 + seed) * 43758.5453
   return v - math.floor(v)
end

---Smooth value noise in [0, 1) with features about `cell` tiles across.
---@param x integer
---@param y integer
---@param cell number
---@param seed number
---@return number
local function noise(x, y, cell, seed)
   local gx, gy = x / cell, y / cell
   local ix, iy = math.floor(gx), math.floor(gy)
   local fx, fy = gx - ix, gy - iy
   fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
   local top = lattice(ix, iy, seed) * (1 - fx) + lattice(ix + 1, iy, seed) * fx
   local bottom = lattice(ix, iy + 1, seed) * (1 - fx) + lattice(ix + 1, iy + 1, seed) * fx
   return top * (1 - fy) + bottom * fy
end

---The tile at a map position inside the map.
---@param x integer
---@param y integer
---@return string
local function terrain(x, y)
   local patch = 0.65 * noise(x, y, 36, 1) + 0.35 * noise(x, y, 14, 2)
   local shade = noise(x, y, 18, 3)
   if patch > SAND_LEVEL then return "sand-" .. (math.floor(shade * 3) + 1) end
   return "grass-" .. (math.floor(shade * 4) + 1)
end

---@param tile_x number
---@param tile_y number
---@return boolean
local function inside(tile_x, tile_y)
   return tile_x >= -HALF and tile_x < HALF and tile_y >= -HALF and tile_y < HALF
end

local function build()
   local surface = game.surfaces.nauvis

   local settings = surface.map_gen_settings
   settings.width = SIZE
   settings.height = SIZE
   surface.map_gen_settings = settings

   surface.request_to_generate_chunks({ 0, 0 }, CHUNK_RADIUS)
   surface.force_generate_chunk_requests()

   local tiles = {}
   for chunk in surface.get_chunks() do
      local left, top = chunk.x * 32, chunk.y * 32
      local overlaps = left < HALF and left + 31 >= -HALF and top < HALF and top + 31 >= -HALF
      if not overlaps then
         surface.delete_chunk({ chunk.x, chunk.y })
      else
         for _, entity in pairs(surface.find_entities_filtered({ area = chunk.area })) do
            if entity.type ~= "character" then entity.destroy() end
         end
         for x = left, left + 31 do
            for y = top, top + 31 do
               local name
               if inside(x, y) then
                  name = terrain(x, y)
               else
                  name = "out-of-map"
               end
               tiles[#tiles + 1] = { name = name, position = { x, y } }
            end
         end
      end
   end
   surface.set_tiles(tiles, false)
   surface.destroy_decoratives({})

   local water = {}
   for x = POND.x, POND.x + POND.width - 1 do
      for y = POND.y, POND.y + POND.height - 1 do
         water[#water + 1] = { name = "water", position = { x, y } }
      end
   end
   surface.set_tiles(water)

   game.forces.player.chart(surface, { { -HALF, -HALF }, { HALF, HALF } })
   print("fabase: built " .. #tiles .. " tiles")
end

commands.add_command("fabase", "Build the Factorio Access campaign base map", build)
