local TestRegistry = require("scripts.test-registry")
local describe, it = TestRegistry.describe, TestRegistry.it
local FaInfo = require("scripts.fa-info")
local TileKinds = require("scripts.tile-kinds")

---The count said for each name in an area summary, by the name's locale key.
---@param message any
---@param found table<string, string>
---@return table<string, string>
local function counts_in(message, found)
   if type(message) ~= "table" then return found end
   if message[1] == "fa.area-scan-item-with-percent" or message[1] == "fa.area-scan-item" then
      found[message[3][1]] = message[2]
   end
   for i = 2, #message do
      counts_in(message[i], found)
   end
   return found
end

describe("Area summary", function()
   it("counts water-like tiles by the game's name of each", function(ctx)
      ---@type LuaPlayer, fa.Point, fa.Point, Tile[]
      local player, left_top, right_bottom, old
      -- Lava is Space Age's; water and shallow water are the base game's
      local space_age = script.active_mods["space-age"] ~= nil
      local lava = space_age and "lava" or "water"

      ctx:init(function()
         player = assert(game.get_player(1))
         local position = player.position --[[@as MapPosition.struct]]
         left_top = { x = math.floor(position.x) + 20, y = math.floor(position.y) + 20 }
         right_bottom = { x = left_top.x + 8, y = left_top.y + 8 }
         old = {}
         local tiles = {}
         for x = left_top.x, right_bottom.x - 1 do
            for y = left_top.y, right_bottom.y - 1 do
               table.insert(old, { name = player.surface.get_tile(x, y).name, position = { x, y } })
               -- Three columns of water, two of shallow water, one of lava; the rest stays land
               local column = x - left_top.x
               local name = column < 3 and "water" or column < 5 and "water-shallow" or column < 6 and lava or nil
               if name then table.insert(tiles, { name = name, position = { x, y } }) end
            end
         end
         player.surface.set_tiles(tiles)
      end)

      ctx:at_tick(2, function()
         local counts = counts_in(FaInfo.area_scan_summary_info(1, left_top, right_bottom), {})
         if space_age then
            ctx:assert_equals("24", counts["tile-name.water"])
            ctx:assert_equals("8", counts["tile-name.lava"])
         else
            ctx:assert_equals("32", counts["tile-name.water"])
         end
         ctx:assert_equals("16", counts["tile-name.water-shallow"])
         player.surface.set_tiles(old)
      end)
   end)

   it("knows water-like tiles by their collision layers", function(ctx)
      ctx:at_tick(1, function()
         ---@type table<string, true?>
         local water_like = {}
         for _, name in ipairs(TileKinds.water_like()) do
            water_like[name] = true
         end
         ctx:assert(water_like["water"], "water is water-like")
         ctx:assert(water_like["deepwater"], "deep water is water-like")
         ctx:assert(water_like["water-shallow"], "shallow water is water-like")
         ctx:assert(not water_like["out-of-map"], "out-of-map is not")
         ctx:assert(not water_like["grass-1"], "grass is not")
         if script.active_mods["space-age"] ~= nil then
            ctx:assert(water_like["lava"], "lava is water-like")
            ctx:assert(water_like["oil-ocean-deep"], "the oil ocean is water-like")
            ctx:assert(not water_like["empty-space"], "empty space is not")
         end
      end)
   end)
end)
