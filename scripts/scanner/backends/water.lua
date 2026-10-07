local Clusterer = require("ds.clusterer")
local FaUtils = require("scripts.fa-utils")
local Localising = require("scripts.localising")
local TileClusterer = require("ds.tile-clusterer")
local Memosort = require("scripts.memosort")
local SC = require("scripts.scanner.scanner-consts")
local TH = require("scripts.table-helpers")

local mod = {}

local WATER_PROTOS_SET = {}
TH.array_to_set(WATER_PROTOS_SET, SC.WATER_PROTOS)

-- An offshore pump stands on the land tile next to the water and faces it. Each facing is the step
-- from that land tile to the water tile.
local PUMP_FACINGS = {
   { x = 0, y = -1, direction = defines.direction.north },
   { x = 1, y = 0, direction = defines.direction.east },
   { x = 0, y = 1, direction = defines.direction.south },
   { x = -1, y = 0, direction = defines.direction.west },
}

---A water entry has aabb; a spot for the offshore pump in hand has pump and direction instead.
---@class fa.scanner.backends.WaterBackendData
---@field aabb fa.AABB?
---@field pump LuaEntityPrototype?
---@field direction defines.direction?

---@class fa.scanner.WaterBackend: fa.scanner.ScannerBackend
---@field surface LuaSurface
---@field clusterer fa.ds.TileClusterer
local WaterBackend = {}
mod.WaterBackend = WaterBackend
local WaterBackend_meta = { __index = WaterBackend }
if script then script.register_metatable("fa.scanner.WaterBackend", WaterBackend_meta) end

---@param surface LuaSurface
function WaterBackend.new(surface)
   return setmetatable(
      { surface = surface, clusterer = TileClusterer.TileClusterer.new({ track_interior = false }), entry_cache = {} },
      WaterBackend_meta
   )
end

---@param e LuaEntity
function WaterBackend:on_new_entity(e) end

---@param player LuaPlayer
---@param pump LuaEntityPrototype
---@param position MapPosition
---@param direction defines.direction
local function can_place_pump(player, pump, position, direction)
   return player.surface.can_place_entity({
      name = pump.name,
      position = position,
      direction = direction,
      force = player.force,
      build_check_type = defines.build_check_type.manual,
   })
end

---The offshore pump the player holds, if any.
---@param player LuaPlayer
---@return LuaEntityPrototype?
local function pump_in_hand(player)
   local stack = player.cursor_stack
   if not stack or not stack.valid_for_read then return nil end
   local place = stack.prototype.place_result
   if place and place.type == "offshore-pump" then return place end
   return nil
end

---@param player LuaPlayer
---@param e fa.scanner.ScanEntry
function WaterBackend:validate_entry(player, e)
   if player.surface.index ~= self.surface.index then return false end

   local data = e.backend_data
   if data.pump then return can_place_pump(player, data.pump, e.position, data.direction) end

   -- Could be landfilled. Check that to get our answer.
   return player.surface.count_tiles_filtered({
      area = e.backend_data.aabb,
      name = SC.WATER_PROTOS,
      limit = 1,
   }) > 0
end

function WaterBackend:update_entry(player, e) end

function WaterBackend:readout_entry(player, e)
   local data = e.backend_data
   if data.pump then
      return {
         "fa.scanner-pump-spot",
         Localising.get_localised_name_with_fallback(data.pump),
         FaUtils.direction_lookup(data.direction),
      }
   end

   local bb = data.aabb
   local w = bb.right_bottom.x - bb.left_top.x
   local h = bb.right_bottom.y - bb.left_top.y
   return { "fa.scanner-water", w, h }
end

---@param player LuaPlayer
---@param callback fun(fa.scanner.ScanEntry)
function WaterBackend:dump_entries_to_callback(player, callback)
   ---@param group fa.ds.TileClusterer.Group
   self.clusterer:get_groups(function(group)
      local tlx = math.huge
      local tly = math.huge
      local brx = -math.huge
      local bry = -math.huge

      local closest_dist = math.huge
      local e_x, e_y
      local px, py = player.position.x, player.position.y

      for x, children in pairs(group.edge_tiles) do
         tlx = tlx < x and tlx or x
         brx = brx > x and brx or x

         for y in pairs(children) do
            tly = tly < y and tly or y
            bry = bry > y and bry or y

            local dist = (px - x) ^ 2 + (py - y) ^ 2
            if dist < closest_dist then
               closest_dist = dist
               e_x = x
               e_y = y
            end
         end
      end

      -- This is the top-left corner of the bottom-right tile. We want the
      -- bottom right.
      brx = brx + 1
      bry = bry + 1

      callback({
         -- This is fun.  If we use the corner of the tile, confused geometry in
         -- the cursor handling code will currently corrupt the cursor to
         -- temporarily be off by one tile to the northwest in some and only
         -- some contexts.  Since this won't break in future, we offset to the
         -- center of the tile to fix that.
         position = { x = e_x + 0.5, y = e_y + 0.5 },
         backend_data = {
            aabb = {
               left_top = { x = tlx, y = tly },
               right_bottom = { x = brx, y = bry },
            },
         },
         backend = self,
         category = SC.CATEGORIES.RESOURCES,
         subcategory = "water",
      })
   end)

   local pump = pump_in_hand(player)
   if pump then self:dump_pump_spots(player, pump, callback) end
end

---Every land tile near the player where the pump can be built facing the water next to it. The game
---turns the pump to face the water itself, so the direction is only read out.
---@param player LuaPlayer
---@param pump LuaEntityPrototype
---@param callback fun(fa.scanner.ScanEntry)
function WaterBackend:dump_pump_spots(player, pump, callback)
   local px, py = player.position.x, player.position.y
   local max_dist_squared = SC.PUMP_SPOT_DISTANCE ^ 2
   ---@type table<string, true>
   local found = {}

   ---@param group fa.ds.TileClusterer.Group
   self.clusterer:get_groups(function(group)
      for x, children in pairs(group.edge_tiles) do
         for y in pairs(children) do
            if (px - x) ^ 2 + (py - y) ^ 2 < max_dist_squared then
               for _, facing in ipairs(PUMP_FACINGS) do
                  local land_x, land_y = x - facing.x, y - facing.y
                  local key = land_x .. "," .. land_y
                  local position = { x = land_x + 0.5, y = land_y + 0.5 }
                  if not found[key] and can_place_pump(player, pump, position, facing.direction) then
                     found[key] = true
                     callback({
                        position = position,
                        backend_data = { pump = pump, direction = facing.direction },
                        backend = self,
                        category = SC.CATEGORIES.BUILD_SPOTS,
                        subcategory = "offshore-pump-spot",
                     })
                  end
               end
            end
         end
      end
   end)
end

---@param chunk ChunkPositionAndArea
function WaterBackend:on_new_chunk(chunk)
   local tiles = self.surface.find_tiles_filtered({
      area = chunk.area,
      name = SC.WATER_PROTOS,
   })

   -- Convert to xy.
   local xy = {}
   for i = 1, #tiles do
      table.insert(xy, tiles[i].position)
   end

   self.clusterer:submit_points(xy)
end

function WaterBackend:get_aabb(e)
   local aabb = e.backend_data.aabb
   local lt = aabb.left_top
   local rb = aabb.right_bottom
   return lt.x, lt.y, rb.x, rb.y
end

function WaterBackend:is_huge(e)
   return not e.backend_data.pump
end

return mod
