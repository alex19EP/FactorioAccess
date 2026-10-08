local TestRegistry = require("scripts.test-registry")
local describe, it = TestRegistry.describe, TestRegistry.it
local FaInfo = require("scripts.fa-info")
local MapOverlays = require("scripts.map-overlays")
local Speech = require("scripts.speech")

---Every localised string of `message` with its key, so a test can find one by key.
---@param message LocalisedString
---@param found table<string, any[]>
local function keys_in(message, found)
   if type(message) ~= "table" then return found end
   if type(message[1]) == "string" and message[1] ~= "" then found[message[1]] = { table.unpack(message, 2) } end
   for i = 2, #message do
      keys_in(message[i], found)
   end
   return found
end

---Builds, in the 16 tile cell east of the player, a small pole, a gun turret, an assembler making
---gears, a pipe of water and a roboport. Returns the cell's corners and everything built.
---@param player LuaPlayer
local function build(player)
   local left = math.floor(player.position.x / 16) * 16 + 32
   local top = math.floor(player.position.y / 16) * 16
   local function create(name, x, y)
      local e = player.surface.create_entity({ name = name, position = { left + x, top + y }, force = player.force })
      assert(e, name .. " not built")
      return e
   end
   local built = {
      create("small-electric-pole", 2.5, 2.5),
      create("gun-turret", 8, 8),
      create("assembling-machine-1", 4.5, 12.5),
      create("pipe", 12.5, 2.5),
      create("roboport", 12, 12),
   }
   built[3].set_recipe("iron-gear-wheel")
   built[4].set_fluid(1, { name = "water", amount = 50 })
   return { x = left, y = top }, { x = left + 16, y = top + 16 }, built
end

describe("Map overlays", function()
   it("says what the overlays that are on show over a cell", function(ctx)
      local player, left_top, right_bottom, built

      ctx:init(function()
         player = game.get_player(1)
         left_top, right_bottom, built = build(player)
      end)

      ctx:at_tick(2, function()
         local message = Speech.MessageBuilder.new()
         MapOverlays.describe(message, player, left_top, right_bottom, {
            electric_network = true,
            logistic_network = true,
            turret_range = true,
            recipe_icons = true,
            pipelines = true,
         })
         local found = keys_in(message:build(), {})
         ctx:assert_equals(1, found["fa.map-overlay-electric-networks"][1])
         ctx:assert_equals(1, found["fa.map-overlay-logistic-networks"][1])
         ctx:assert_equals(1, found["fa.map-overlay-turrets"][1])
         ctx:assert_equals(1, found["fa.map-overlay-count"][1])
         ctx:assert_not_nil(found["fa.map-overlay-making"])
         ctx:assert_not_nil(found["fa.map-overlay-pipes"])
      end)

      ctx:at_tick(3, function()
         for _, e in ipairs(built) do
            e.destroy()
         end
      end)
   end)

   it("leaves worker robots out of a cell's count when asked", function(ctx)
      local player, left_top, right_bottom, robot

      ctx:init(function()
         player = game.get_player(1)
         left_top = { x = math.floor(player.position.x / 16) * 16 + 64, y = math.floor(player.position.y / 16) * 16 }
         right_bottom = { x = left_top.x + 16, y = left_top.y + 16 }
         robot = player.surface.create_entity({
            name = "construction-robot",
            position = { left_top.x + 8, left_top.y + 8 },
            force = player.force,
         })
      end)

      ctx:at_tick(2, function()
         local with = keys_in(FaInfo.area_scan_summary_info(1, left_top, right_bottom), {})
         ctx:assert_not_nil(with["entity-name.construction-robot"])
         local without =
            keys_in(FaInfo.area_scan_summary_info(1, left_top, right_bottom, MapOverlays.WORKER_ROBOT_TYPES), {})
         ctx:assert_nil(without["entity-name.construction-robot"])
         robot.destroy()
      end)
   end)

   it("says nothing while every overlay is off", function(ctx)
      local player, left_top, right_bottom, built

      ctx:init(function()
         player = game.get_player(1)
         left_top, right_bottom, built = build(player)
      end)

      ctx:at_tick(2, function()
         local message = Speech.MessageBuilder.new()
         MapOverlays.describe(message, player, left_top, right_bottom, {})
         ctx:assert_equals(nil, next(keys_in(message:build(), {})))
      end)

      ctx:at_tick(3, function()
         for _, e in ipairs(built) do
            e.destroy()
         end
      end)
   end)
end)
