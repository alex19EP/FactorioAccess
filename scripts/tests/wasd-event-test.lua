--- A movement key's tile is read by the DLL as the game reads the key (native/src/reader.h); the
--- mod's handler only sounds the step the DLL made, and keeps no cursor of its own.

local Cursor = require("scripts.cursor")
local EventManager = require("scripts.event-manager")
local Speech = require("scripts.speech")
local TestRegistry = require("scripts.test-registry")
local describe = TestRegistry.describe
local it = TestRegistry.it

describe("WASD Event Tests", function()
   it("leaves reading the tile to the DLL", function(ctx)
      local pindex = 1
      local player, chest, tile

      ctx:init(function()
         player = game.get_player(pindex)
         local x, y = math.floor(player.position.x), math.floor(player.position.y)
         tile = { x = x + 3, y = y }
         chest = player.surface.create_entity({
            name = "wooden-chest",
            position = { tile.x + 0.5, tile.y + 0.5 },
            force = player.force,
         })
      end)

      ctx:at_tick(1, function()
         -- An event the DLL did not move the cursor for: no step of its own to sound or read
         Speech.start_capture()
         EventManager.mock_event("fa-d", {
            player_index = pindex,
            cursor_position = { x = tile.x + 0.5, y = tile.y + 0.5 },
         })
         ctx:assert_equals(0, #Speech.stop_capture(), "The mod does not read the tile itself")
         ctx:assert(not Cursor.known(pindex), "The cursor is known only while the key is handled")
         chest.destroy()
      end)
   end)
end)
