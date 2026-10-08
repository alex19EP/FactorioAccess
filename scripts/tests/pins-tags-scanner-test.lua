local TestRegistry = require("scripts.test-registry")
local describe, it = TestRegistry.describe, TestRegistry.it
local ChartTagsBackend = require("scripts.scanner.backends.chart-tags")
local PinsBackend = require("scripts.scanner.backends.pins")
local SC = require("scripts.scanner.scanner-consts")

---@param backend fa.scanner.ScannerBackend
---@param player LuaPlayer
---@return fa.scanner.ScanEntry[]
local function entries_of(backend, player)
   local entries = {}
   backend:dump_entries_to_callback(player, function(e)
      table.insert(entries, e)
   end)
   return entries
end

---Whether a localised string holds `wanted` anywhere inside it.
---@param ls LocalisedString
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
      local player, chest, ore, ore_pin

      ctx:init(function()
         player = game.get_player(1)
         player.clear_pins()
         local x, y = math.floor(player.position.x), math.floor(player.position.y)
         chest = player.surface.create_entity({ name = "wooden-chest", position = { x + 5, y }, force = player.force })
         ore = {
            player.surface.create_entity({ name = "iron-ore", position = { x - 8, y - 8 }, amount = 300 }),
            player.surface.create_entity({ name = "iron-ore", position = { x - 7, y - 8 }, amount = 200 }),
         }
         player.add_pin({ label = "Home", surface = player.surface, position = { x + 3, y + 3 } })
         player.add_pin({ entity = chest })
         ore_pin = player.add_pin({ entity = ore[1] })
         ore_pin.targets = ore
      end)

      ctx:at_tick(2, function()
         local entries = entries_of(PinsBackend.PinsBackend.new(), player)
         ctx:assert_equals(3, #entries, "Each pin on the surface is listed")

         local readouts = {}
         for _, e in ipairs(entries) do
            ctx:assert_equals(SC.CATEGORIES.PINS, e.category)
            ctx:assert(e.backend:validate_entry(player, e), "A listed pin is valid")
            readouts[e.backend_data.index] = e.backend:readout_entry(player, e)
         end

         local found_label, found_chest, found_ore = false, false, false
         for _, readout in pairs(readouts) do
            ctx:assert(contains(readout, "fa.scanner-pin"), "Every readout says it is a pin")
            if contains(readout, "Home") then found_label = true end
            if contains(readout, "entity-name.wooden-chest") then found_chest = true end
            if contains(readout, "fa.scanner-pin-resource") and contains(readout, "500") then found_ore = true end
         end
         ctx:assert(found_label, "A labelled pin reads its label")
         ctx:assert(found_chest, "An entity pin reads the entity")
         ctx:assert(found_ore, "A resource patch pin reads what is left of it")

         chest.destroy()
         for _, e in ipairs(ore) do
            e.destroy()
         end
         player.clear_pins()
      end)
   end)

   it("lists the force's map tags with their text", function(ctx)
      local player, tag

      ctx:init(function()
         player = game.get_player(1)
         tag = player.force.add_chart_tag(player.surface, {
            position = { player.position.x + 4, player.position.y - 4 },
            text = "Smelting",
         })
      end)

      ctx:at_tick(2, function()
         ctx:assert_not_nil(tag, "The tag is placed on charted ground")
         local entries = entries_of(ChartTagsBackend.ChartTagsBackend.new(), player)
         local found = nil
         for _, e in ipairs(entries) do
            if e.backend_data == tag then found = e end
         end
         ctx:assert_not_nil(found, "The tag is listed")
         ctx:assert_equals(SC.CATEGORIES.TAGS, found.category)
         ctx:assert(found.backend:validate_entry(player, found), "A listed tag is valid")
         local readout = found.backend:readout_entry(player, found)
         ctx:assert(contains(readout, "Smelting"), "A tag reads its text")
         ctx:assert(contains(readout, "fa.scanner-tag"), "A tag readout says it is a map tag")

         tag.destroy()
         ctx:assert(not found.backend:validate_entry(player, found), "A removed tag is no longer valid")
      end)
   end)
end)
