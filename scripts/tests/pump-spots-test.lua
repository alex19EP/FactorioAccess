local TestRegistry = require("scripts.test-registry")
local describe, it = TestRegistry.describe, TestRegistry.it
local SC = require("scripts.scanner.scanner-consts")
local WaterBackend = require("scripts.scanner.backends.water")

local STEPS = {
   [defines.direction.north] = { x = 0, y = -1 },
   [defines.direction.east] = { x = 1, y = 0 },
   [defines.direction.south] = { x = 0, y = 1 },
   [defines.direction.west] = { x = -1, y = 0 },
}

---Makes a 4 by 4 pond near the player and returns the tiles it replaced, to put back.
---@param player LuaPlayer
local function make_pond(player)
   local left = math.floor(player.position.x) + 6
   local top = math.floor(player.position.y) + 6
   local area = { left_top = { x = left, y = top }, right_bottom = { x = left + 4, y = top + 4 } }
   local old = {}
   local water = {}
   for x = left, left + 3 do
      for y = top, top + 3 do
         table.insert(old, { name = player.surface.get_tile(x, y).name, position = { x, y } })
         table.insert(water, { name = "water", position = { x, y } })
      end
   end
   player.surface.set_tiles(water)
   return area, old
end

---@param player LuaPlayer
---@param area BoundingBox
local function build_spots(player, area)
   local backend = WaterBackend.WaterBackend.new(player.surface)
   backend:on_new_chunk({ x = 0, y = 0, area = area })
   local spots = {}
   backend:dump_entries_to_callback(player, function(e)
      if e.category == SC.CATEGORIES.BUILD_SPOTS then table.insert(spots, e) end
   end)
   return spots
end

describe("Offshore pump build spots", function()
   it("lists land tiles facing the water while a pump is in hand", function(ctx)
      local player, area, old

      ctx:init(function()
         player = game.get_player(1)
         area, old = make_pond(player)
         player.cursor_stack.set_stack({ name = "offshore-pump", count = 1 })
      end)

      ctx:at_tick(2, function()
         local spots = build_spots(player, area)
         ctx:assert(#spots > 0, "A pond next to the player should have pump spots")

         local seen = {}
         for _, spot in ipairs(spots) do
            local x, y = math.floor(spot.position.x), math.floor(spot.position.y)
            local key = x .. "," .. y
            ctx:assert_nil(seen[key], "Each land tile is listed once")
            seen[key] = true

            ctx:assert_not_equals("water", player.surface.get_tile(x, y).name, "A pump stands on land")
            local step = STEPS[spot.backend_data.direction]
            ctx:assert_equals(
               "water",
               player.surface.get_tile(x + step.x, y + step.y).name,
               "A pump faces the water next to it"
            )
            ctx:assert(spot.backend:validate_entry(player, spot), "A listed spot can be built")
            ctx:assert_equals("fa.scanner-pump-spot", spot.backend:readout_entry(player, spot)[1])
         end
      end)

      ctx:at_tick(3, function()
         player.cursor_stack.clear()
         ctx:assert_equals(0, #build_spots(player, area), "No spots without a pump in hand")
         player.surface.set_tiles(old)
      end)
   end)
end)
