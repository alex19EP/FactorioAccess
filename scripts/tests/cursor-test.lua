--- The cursor as the mod sees it (scripts/cursor.lua): the DLL owns it, and the mod knows its tile only
--- while a key event that carries it is handled, or inside Cursor.at.

local Cursor = require("scripts.cursor")
local EventManager = require("scripts.event-manager")
local TestRegistry = require("scripts.test-registry")
local describe = TestRegistry.describe
local it = TestRegistry.it

-- What a test does inside a key's handlers. The game knows only declared inputs, so the tests
-- watch a real key, and the watcher does nothing outside them.
---@type fun(pindex: integer)?
local on_key

EventManager.on_event("fa-d", function(event)
   if on_key then on_key(event.player_index) end
end, EventManager.EVENT_KIND.TEST)

---@param position MapPosition
---@param fn fun(pindex: integer)
local function press(position, fn)
   on_key = fn
   EventManager.mock_event("fa-d", { player_index = 1, cursor_position = position })
   on_key = nil
end

describe("Cursor", function()
   it("gives a key's handlers the tile its cursor_position is on", function(ctx)
      ctx:at_tick(1, function()
         ---@type { first: fa.Tile, again: fa.Tile }, boolean
         local seen, known_inside
         press({ x = -2.5, y = 7.25 }, function(pindex)
            known_inside = Cursor.known(pindex)
            local first = Cursor.tile(pindex)
            -- The tile is a copy
            first.x = first.x + 100
            seen = { first = first, again = Cursor.tile(pindex) }
         end)
         ctx:assert(known_inside, "The tile is known while the key is handled")
         ctx:assert_equals(-3, seen.again.x, "Negative coordinates round down to the tile")
         ctx:assert_equals(7, seen.again.y)
         ctx:assert_equals(97, seen.first.x, "Changing a returned tile leaves the cursor's alone")
         ctx:assert(not Cursor.known(1), "The tile is forgotten when the key's handlers end")
         ctx:assert(not pcall(Cursor.tile, 1), "Asking for the tile outside a key event is an error")
      end)
   end)

   it("moves the tile with a jump the handler makes", function(ctx)
      ctx:at_tick(1, function()
         local after
         press({ x = 0.5, y = 0.5 }, function(pindex)
            Cursor.jump(pindex, { x = 10.5, y = -4.5 })
            after = Cursor.tile(pindex)
         end)
         ctx:assert_equals(10, after.x, "The rest of the handler sees where the cursor went")
         ctx:assert_equals(-5, after.y)
         ctx:assert(not Cursor.known(1), "The jump does not outlive the key")
      end)
   end)

   it("names a tile outside key events with Cursor.at", function(ctx)
      ctx:at_tick(1, function()
         local outer, inner, restored
         Cursor.at(1, { x = 1.5, y = 2.5 }, function()
            outer = Cursor.tile(1)
            Cursor.at(1, { x = 5.5, y = -5.5 }, function()
               inner = Cursor.tile(1)
            end)
            restored = Cursor.tile(1)
         end)
         ctx:assert_equals(1, outer.x)
         ctx:assert_equals(2, outer.y)
         ctx:assert_equals(5, inner.x, "An inner Cursor.at names its own tile")
         ctx:assert_equals(-6, inner.y)
         ctx:assert_equals(1, restored.x, "and gives the outer one back when it ends")
         ctx:assert_equals(2, restored.y)
         ctx:assert(not Cursor.known(1), "The tile is forgotten after Cursor.at")
      end)
   end)

   it("leaves the tile unknown after a jump outside a key event", function(ctx)
      ctx:at_tick(1, function()
         Cursor.jump(1, { x = 3.5, y = 3.5 })
         ctx:assert(not Cursor.known(1), "Only key events and Cursor.at make the tile known")
      end)
   end)
end)
