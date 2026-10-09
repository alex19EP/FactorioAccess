local TestRegistry = require("scripts.test-registry")
local describe, it = TestRegistry.describe, TestRegistry.it
local InventoryUtils = require("scripts.inventory-utils")

describe("God mode", function()
   it("places from the god's inventory", function(ctx)
      local player, character

      ctx:init(function()
         player = game.get_player(1)
         character = player.character
      end)

      -- As the sandbox scenario leaves the player, without a character
      ctx:at_tick(1, function()
         player.set_controller({ type = defines.controllers.god })
      end)

      ctx:at_tick(2, function()
         ctx:assert_equals(defines.controllers.god, player.controller_type)
         ctx:assert_equals(nil, InventoryUtils.deductor_to_place(1, "straight-rail", true), "No rails, no deductor")
         player.get_main_inventory().insert({ name = "rail", count = 3 })
         local deductor = InventoryUtils.deductor_to_place(1, "straight-rail", true)
         ctx:assert(deductor ~= nil, "Rails in the god's inventory can be placed")
         deductor:commit()
         ctx:assert_equals(2, player.get_main_inventory().get_item_count("rail"), "Placing takes a rail")
         player.get_main_inventory().clear()
         player.set_controller({ type = defines.controllers.character, character = character })
      end)
   end)
end)
