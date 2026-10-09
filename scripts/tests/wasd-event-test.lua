--- A movement key reads where its event says the cursor is: the DLL moves the cursor as the game
--- reads the key, so the event already carries the new tile. The mod keeps no cursor of its own.

local Cursor = require("scripts.cursor")
local EventManager = require("scripts.event-manager")
local Speech = require("scripts.speech")
local TestRegistry = require("scripts.test-registry")
local describe = TestRegistry.describe
local it = TestRegistry.it

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

describe("WASD Event Tests", function()
   it("reads the tile the key's event carries", function(ctx)
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
         Speech.start_capture()
         EventManager.mock_event("fa-d", {
            player_index = pindex,
            cursor_position = { x = tile.x + 0.5, y = tile.y + 0.5 },
         })
         local found = false
         for _, m in ipairs(Speech.stop_capture()) do
            if contains(m.message, "entity-name.wooden-chest") then found = true end
         end
         ctx:assert(found, "The chest at the event's cursor position is read")
         ctx:assert(not Cursor.known(pindex), "The cursor is known only while the key is handled")
         chest.destroy()
      end)
   end)
end)
