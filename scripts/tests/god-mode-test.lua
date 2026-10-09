local TestRegistry = require("scripts.test-registry")
local describe, it = TestRegistry.describe, TestRegistry.it
local InventoryUtils = require("scripts.inventory-utils")
local MovementHistory = require("scripts.movement-history")
local Viewpoint = require("scripts.viewpoint")

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

   it("walks with the anchored cursor one tile ahead", function(ctx)
      local player, character, start

      ctx:init(function()
         player = game.get_player(1)
         character = player.character
      end)

      ctx:at_tick(1, function()
         player.set_controller({ type = defines.controllers.god })
         Viewpoint.get_viewpoint(1):set_cursor_anchored(true)
         start = player.physical_position
      end)

      -- The game walks a player only while its walking state says so, as a held key does
      for tick = 2, 20 do
         ctx:at_tick(tick, function()
            player.walking_state = { walking = true, direction = defines.direction.east }
         end)
      end

      ctx:at_tick(21, function()
         local entry = MovementHistory.get_movement_history_reader(1):get(0)
         ctx:assert_equals(MovementHistory.MOVEMENT_KINDS.WALKING, entry.kind, "God mode walking is recorded")
         ctx:assert(entry.position.x > start.x + 3, "The history follows the god")
         player.walking_state = { walking = false, direction = defines.direction.east }
      end)

      ctx:at_tick(24, function()
         local pos = player.physical_position
         local cursor = Viewpoint.get_viewpoint(1):get_cursor_pos()
         ctx:assert_equals(math.floor(pos.x) + 1, cursor.x, "The anchored cursor is a tile ahead")
         ctx:assert_equals(math.floor(pos.y), cursor.y, "on the god's row")
         Viewpoint.get_viewpoint(1):set_cursor_anchored(false)
         player.set_controller({ type = defines.controllers.character, character = character })
      end)
   end)
end)
