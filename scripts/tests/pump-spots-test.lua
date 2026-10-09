local TestRegistry = require("scripts.test-registry")
local describe, it = TestRegistry.describe, TestRegistry.it
local Extras = require("scripts.scanner.extras")
local SC = require("scripts.scanner.scanner-consts")

---Makes a 4 by 4 pond near the player and returns the tiles it replaced, to put back.
---@param player LuaPlayer
local function make_pond(player)
   local left = math.floor(player.position.x) + 6
   local top = math.floor(player.position.y) + 6
   local old = {}
   local water = {}
   for x = left, left + 3 do
      for y = top, top + 3 do
         table.insert(old, { name = player.surface.get_tile(x, y).name, position = { x, y } })
         table.insert(water, { name = "water", position = { x, y } })
      end
   end
   player.surface.set_tiles(water)
   return old
end

-- The step from a pump's land tile to the water it faces, by the direction it faces.
local STEPS = {
   [defines.direction.north] = { x = 0, y = -1 },
   [defines.direction.east] = { x = 1, y = 0 },
   [defines.direction.south] = { x = 0, y = 1 },
   [defines.direction.west] = { x = -1, y = 0 },
}

describe("Offshore pump build spots", function()
   it("lists land tiles facing the water while a pump is in hand", function(ctx)
      local player, old

      ctx:init(function()
         player = game.get_player(1)
         old = make_pond(player)
         player.cursor_stack.set_stack({ name = "offshore-pump", count = 1 })
      end)

      ctx:at_tick(2, function()
         local spots = Extras.pump_spots(player)
         ctx:assert(#spots > 0, "A pond next to the player should have pump spots")

         local seen = {}
         for _, spot in ipairs(spots) do
            ctx:assert_equals(SC.CATEGORIES.BUILD_SPOTS, spot.category)
            local x, y = math.floor(spot.position.x), math.floor(spot.position.y)
            local key = x .. "," .. y
            ctx:assert_nil(seen[key], "Each land tile is listed once")
            seen[key] = true

            ctx:assert_not_equals("water", player.surface.get_tile(x, y).name, "A pump stands on land")
            local readout = spot.readout()
            ctx:assert_equals("fa.scanner-pump-spot", readout[1])
            ctx:assert(spot.valid(), "A listed spot can be built")
         end

         -- Each spot faces water: the direction it reads is toward a water tile.
         for _, spot in ipairs(spots) do
            local x, y = math.floor(spot.position.x), math.floor(spot.position.y)
            local faces_water = false
            for _, step in pairs(STEPS) do
               if player.surface.get_tile(x + step.x, y + step.y).name == "water" then faces_water = true end
            end
            ctx:assert(faces_water, "A pump spot is next to the water")
         end
      end)

      ctx:at_tick(3, function()
         player.cursor_stack.clear()
         ctx:assert_equals(0, #Extras.pump_spots(player), "No spots without a pump in hand")
         player.surface.set_tiles(old)
      end)
   end)
end)
