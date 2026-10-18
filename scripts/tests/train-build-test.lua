-- What building rolling stock or a train stop says (scripts/train-build.lua).
local TestRegistry = require("scripts.test-registry")
local describe, it = TestRegistry.describe, TestRegistry.it
local Speech = require("scripts.speech")
local TrainBuild = require("scripts.train-build")

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

---What the build of `entity` says.
---@param entity LuaEntity
---@return LocalisedString
local function built(entity)
   Speech.start_capture()
   -- The handler reads the entity alone.
   ---@diagnostic disable-next-line: missing-fields, param-type-mismatch
   TrainBuild.on_built_entity({ entity = entity }, 1)
   local said = Speech.stop_capture()
   return said[1] and said[1].message
end

describe("Train build", function()
   it("says whether rolling stock joined a train, and a stop's way", function(ctx)
      ---@type LuaEntity
      local locomotive
      ---@type LuaEntity
      local wagon
      ---@type LuaEntity
      local lone
      ---@type LuaEntity
      local stop
      ---@type LuaEntity
      local at_stop

      ctx:init(function()
         local player = assert(game.get_player(1))
         local surface = player.surface
         surface.request_to_generate_chunks({ x = 0, y = 320 }, 4)
         surface.force_generate_chunk_requests()
         for x = 1, 41, 2 do
            for _, y in ipairs({ 301, 321, 341 }) do
               assert(surface.create_entity({
                  name = "straight-rail",
                  position = { x = x, y = y },
                  direction = defines.direction.east,
                  force = player.force,
               }))
            end
         end
         stop = assert(surface.create_entity({
            name = "train-stop",
            position = { x = 31, y = 299 },
            direction = defines.direction.east,
            force = player.force,
         }))
         stop.backer_name = "Test stop"
         locomotive = assert(surface.create_entity({
            name = "locomotive",
            position = { x = 21, y = 301 },
            direction = defines.direction.east,
            force = player.force,
         }))
         wagon = assert(surface.create_entity({
            name = "cargo-wagon",
            position = { x = 14, y = 301 },
            direction = defines.direction.east,
            force = player.force,
         }))
         lone = assert(surface.create_entity({
            name = "locomotive",
            position = { x = 21, y = 321 },
            direction = defines.direction.west,
            force = player.force,
         }))
         local platform = assert(surface.create_entity({
            name = "train-stop",
            position = { x = 31, y = 343 },
            direction = defines.direction.east,
            force = player.force,
         }))
         platform.backer_name = "Platform"
         -- Its front at the stop, as the game snaps a locomotive built there.
         at_stop = assert(surface.create_entity({
            name = "locomotive",
            position = { x = 28, y = 341 },
            direction = defines.direction.east,
            force = player.force,
         }))
      end)

      ctx:at_tick(2, function()
         local joined = built(wagon)
         ctx:assert(contains(joined, "fa.train-build-connected"), "The wagon joined the locomotive")
         ctx:assert(contains(joined, 2), "The train has two cars")
         local alone = built(lone)
         ctx:assert(contains(alone, "fa.train-build-not-connected"), "A locomotive on its own joins nothing")
         ctx:assert(contains(alone, "fa.facing-direction"), "The way it heads")
         ctx:assert(not contains(joined, "fa.train-build-at-stop"), "A train far from the stop is at none")
         local parked = built(at_stop)
         ctx:assert(contains(parked, "fa.train-build-at-stop"), "A locomotive built at a stop says it")
         ctx:assert(contains(parked, "Platform"), "The stop's name")
         local stopped = built(stop)
         ctx:assert(contains(stopped, "fa.train-build-stop"), "A stop says its name and way")
         ctx:assert(contains(stopped, "Test stop"), "The stop's name")
         ctx:assert(locomotive.train == wagon.train, "Both cars are one train")
      end)
   end)
end)
