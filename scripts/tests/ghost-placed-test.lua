local TestRegistry = require("scripts.test-registry")
local describe, it = TestRegistry.describe, TestRegistry.it
local Speech = require("scripts.speech")

describe("Ghost placed", function()
   it("says a ghost placed from the hand", function(ctx)
      local player, position

      ctx:init(function()
         player = game.get_player(1)
         position = { x = math.floor(player.position.x) + 4.5, y = math.floor(player.position.y) + 4.5 }
      end)

      ctx:at_tick(1, function()
         player.clear_cursor()
         player.cursor_ghost = "wooden-chest"
         Speech.start_capture()
         player.build_from_cursor({ position = position })
         local said = Speech.stop_capture()
         local ghost = player.surface.find_entity("entity-ghost", position)
         ctx:assert_not_nil(ghost, "The ghost is built")
         local found = false
         for _, m in ipairs(said) do
            if type(m.message) == "table" and m.message[1] == "fa.building-placed-ghost" then found = true end
         end
         ctx:assert(found, "Placing the ghost is said")
         ghost.destroy()
         player.clear_cursor()
      end)
   end)
end)
