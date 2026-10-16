local TestRegistry = require("scripts.test-registry")
local describe, it = TestRegistry.describe, TestRegistry.it
local Readout = require("scripts.scanner.readout")

describe("Scanner alerts", function()
   it("says an alert by its type, a custom one by its message, then what it is about", function(ctx)
      ---@type LuaPlayer, LuaEntity
      local player, chest

      ctx:init(function()
         player = assert(game.get_player(1))
         local position = player.position --[[@as MapPosition.struct]]
         chest = assert(player.surface.create_entity({
            name = "wooden-chest",
            position = { position.x + 5, position.y + 5 },
            force = player.force,
         }))
         player.add_custom_alert(chest, { type = "item", name = "wooden-chest" }, "Test alert", true)
      end)

      ctx:at_tick(2, function()
         local custom = Readout.alert(player, "custom", chest, "wooden-chest", chest.position)
         ctx:assert_equals("Test alert", custom.words)
         ctx:assert_equals(chest, custom.entity)

         local damaged = Readout.alert(player, "entity_under_attack", chest, "wooden-chest", chest.position)
         ctx:assert_table_equals({ "alerts-config-gui.name-entity_under_attack" }, damaged.words)
         ctx:assert_equals(chest, damaged.entity)

         -- A destroyed entity's alert has no entity left, only what it was
         local destroyed = Readout.alert(player, "entity_destroyed", nil, "wooden-chest", chest.position)
         ctx:assert_nil(destroyed.entity)
         ctx:assert_not_nil(destroyed.words)
      end)

      ctx:at_tick(3, function()
         player.remove_alert({ type = defines.alert_type.custom })
         chest.destroy()
      end)
   end)
end)
