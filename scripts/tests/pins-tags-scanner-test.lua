local TestRegistry = require("scripts.test-registry")
local describe, it = TestRegistry.describe, TestRegistry.it
local Extras = require("scripts.scanner.extras")
local SC = require("scripts.scanner.scanner-consts")

---Whether a localised string or readout holds `wanted` anywhere inside it.
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

describe("Scanner pins and map tags", function()
   it("lists the player's pins with their labels and targets", function(ctx)
      ---@type LuaPlayer
      local player
      ---@type LuaEntity
      local chest
      ---@type LuaEntity[]
      local ore

      ctx:init(function()
         player = assert(game.get_player(1))
         player.clear_pins()
         local position = player.position --[[@as MapPosition.struct]]
         local x, y = math.floor(position.x), math.floor(position.y)
         chest = assert(
            player.surface.create_entity({ name = "wooden-chest", position = { x + 5, y }, force = player.force })
         )
         local rich =
            assert(player.surface.create_entity({ name = "iron-ore", position = { x - 8, y - 8 }, amount = 300 }))
         local poor =
            assert(player.surface.create_entity({ name = "iron-ore", position = { x - 7, y - 8 }, amount = 200 }))
         ore = { rich, poor }
         player.add_pin({ label = "Home", surface = player.surface, position = { x + 3, y + 3 } })
         player.add_pin({ surface = player.surface, position = { x + 3.5, y - 2.5 } })
         player.add_pin({ entity = chest })
         local ore_pin = player.add_pin({ entity = ore[1] })
         ore_pin.targets = ore
      end)

      ctx:at_tick(2, function()
         local extras = Extras.pins(player)
         ctx:assert_equals(4, #extras, "Each pin on the surface is listed")

         -- As the pins panel names a place: its coordinates cut toward zero
         local position = player.position --[[@as MapPosition.struct]]
         local x, y = math.floor(position.x), math.floor(position.y)
         local function cut(value)
            return value >= 0 and math.floor(value) or math.ceil(value)
         end
         local place = string.format("{%d, %d}", cut(x + 3.5), cut(y - 2.5))

         local found_label, found_place, found_chest, found_ore = false, false, false, false
         for _, extra in ipairs(extras) do
            ctx:assert_equals(SC.CATEGORIES.PINS, extra.category)
            ctx:assert(extra.valid(), "A listed pin is valid")
            local readout = extra.readout()
            ctx:assert(contains(readout, "fa.scanner-pin"), "Every readout says it is a pin")
            if contains(readout, "Home") then
               found_label = true
               ctx:assert(not contains(readout, place), "A labelled pin is named by its label alone")
            end
            if contains(readout, place) then found_place = true end
            -- The DLL's tile reader says the entity
            if readout.entity == chest then found_chest = true end
            if contains(readout, "fa.scanner-pin-resource") and contains(readout, "500") then found_ore = true end
         end
         ctx:assert(found_label, "A labelled pin reads its label")
         ctx:assert(found_place, "An unlabelled place pin reads its coordinates")
         ctx:assert(found_chest, "An entity pin has the entity read")
         ctx:assert(found_ore, "A resource patch pin reads what is left of it")

         chest.destroy()
         for _, e in ipairs(ore) do
            e.destroy()
         end
         player.clear_pins()
      end)
   end)

   it("lists the force's map tags with their text", function(ctx)
      ---@type LuaPlayer
      local player
      ---@type LuaCustomChartTag
      local tag

      ctx:init(function()
         player = assert(game.get_player(1))
         -- nil when the ground is not charted
         tag = assert(player.force.add_chart_tag(player.surface, {
            position = { player.position.x + 4, player.position.y - 4 },
            text = "Smelting",
         }))
      end)

      ctx:at_tick(2, function()
         ---@type fa.scanner.Extra?
         local found = nil
         for _, extra in ipairs(Extras.tags(player)) do
            if extra.position.x == tag.position.x and extra.position.y == tag.position.y then found = extra end
         end
         ctx:assert_not_nil(found, "The tag is listed")
         ctx:assert_equals(SC.CATEGORIES.TAGS, found.category)
         ctx:assert(found.valid(), "A listed tag is valid")
         local readout = found.readout()
         ctx:assert(contains(readout, "Smelting"), "A tag reads its text")
         ctx:assert(contains(readout, "fa.scanner-tag"), "A tag readout says it is a map tag")

         tag.destroy()
         ctx:assert(not found.valid(), "A removed tag is no longer valid")
      end)
   end)

   it("keeps what a pin said once the pin is gone", function(ctx)
      ---@type LuaPlayer
      local player

      ctx:init(function()
         player = assert(game.get_player(1))
         player.clear_pins()
         player.add_pin({
            label = "Gone",
            surface = player.surface,
            position = player.position,
         })
      end)

      ctx:at_tick(2, function()
         local listed = Extras.collect(player, 1)
         ---@type integer?
         local id = nil
         for i, extra in ipairs(listed) do
            if extra.category == SC.CATEGORIES.PINS then id = i end
         end
         ctx:assert_not_nil(id, "The pin is handed to the DLL")
         player.clear_pins()
         ctx:assert(contains(Extras.readout(player.index, 1, assert(id)), "Gone"), "A removed pin reads as it was")
      end)
   end)
end)
