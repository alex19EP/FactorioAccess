-- What a click on the map would add to the schedule of the train the player rides, said after a map
-- cell (scripts/train-map-targets.lua).
local TestRegistry = require("scripts.test-registry")
local describe, it = TestRegistry.describe, TestRegistry.it
local Speech = require("scripts.speech")
local TrainMapTargets = require("scripts.train-map-targets")

---Whether a localised string holds `wanted` anywhere inside it.
---@param ls any
---@param wanted any
local function contains(ls, wanted)
   if ls == wanted then return true end
   if type(ls) ~= "table" then return false end
   for _, part in pairs(ls) do
      if contains(part, wanted) then return true end
   end
   return false
end

---@param player LuaPlayer
---@param x number
---@param y number
---@return LocalisedString
local function targets(player, x, y)
   local message = Speech.MessageBuilder.new()
   TrainMapTargets.describe(message, player, { x = x, y = y })
   return message:build()
end

describe("Train map targets", function()
   it("says the rail and stop a click would add, and whether the train reaches them", function(ctx)
      ---@type LuaPlayer
      local player
      ---@type LuaEntity
      local stop

      ctx:init(function()
         player = assert(game.get_player(1))
         local surface = player.surface
         surface.request_to_generate_chunks({ x = 0, y = 220 }, 4)
         surface.force_generate_chunk_requests()
         for x = 1, 81, 2 do
            assert(surface.create_entity({
               name = "straight-rail",
               position = { x = x, y = 201 },
               direction = defines.direction.east,
               force = player.force,
            }))
         end
         -- A stretch no other rail joins
         for x = 1, 11, 2 do
            assert(surface.create_entity({
               name = "straight-rail",
               position = { x = x, y = 241 },
               direction = defines.direction.east,
               force = player.force,
            }))
         end
         stop = assert(surface.create_entity({
            name = "train-stop",
            position = { x = 61, y = 203 },
            direction = defines.direction.east,
            force = player.force,
         }))
         stop.backer_name = "Test stop"
         local locomotive = assert(surface.create_entity({
            name = "locomotive",
            position = { x = 11, y = 201 },
            direction = defines.direction.east,
            force = player.force,
         }))
         locomotive.set_driver(player)
      end)

      ctx:at_tick(2, function()
         ctx:assert(stop.connected_rail ~= nil, "The stop is on the track")
         local near_stop = targets(player, 60.5, 205.5)
         ctx:assert(contains(near_stop, "fa.map-train-rail-reachable"), "A rail the train reaches")
         ctx:assert(contains(near_stop, "fa.map-train-stop-reachable"), "A stop the train reaches")
         ctx:assert(contains(near_stop, "Test stop"), "The stop's name")

         local apart = targets(player, 6.5, 241.5)
         ctx:assert(contains(apart, "fa.map-train-rail-no-path"), "A rail the train cannot reach")
         ctx:assert(not contains(apart, "fa.map-train-stop-reachable"), "No stop nearby")

         ctx:assert_equals("", targets(player, 6.5, 300.5), "Nothing within 20 tiles")

         player.driving = false
         ctx:assert_equals("", targets(player, 60.5, 205.5), "Off the train, no train to send")
      end)
   end)
end)
