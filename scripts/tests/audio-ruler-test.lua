--- The audio ruler tool (scripts/rulers.lua): the shortcut and Ctrl+Alt+B put it in hand, a click
--- with it places a ruler, a Shift+click removes the nearest one in line, and each is drawn.

local EventManager = require("scripts.event-manager")
local Rulers = require("scripts.rulers")
local TestRegistry = require("scripts.test-registry")
local describe = TestRegistry.describe
local it = TestRegistry.it

local pindex = 1

---A click with the tool in hand. The click takes the tool from the hand until the next tick.
---@param input string
---@param x number
---@param y number
local function click(input, x, y)
   Rulers.give_tool(pindex)
   EventManager.mock_event(input, { player_index = pindex, cursor_position = { x = x, y = y } })
end

local function drawings()
   return #rendering.get_all_objects(script.mod_name)
end

---@param tiles integer[]
---@return string
local function listed(tiles)
   return table.concat(tiles, " ")
end

describe("Audio ruler", function()
   it("is put in hand by its key and its shortcut", function(ctx)
      local player
      ctx:init(function()
         player = game.get_player(pindex)
         player.clear_cursor()
      end)

      ctx:at_tick(1, function()
         EventManager.mock_event("fa-ca-b", { player_index = pindex, cursor_position = { x = 0, y = 0 } })
         ctx:assert_equals(Rulers.TOOL, player.cursor_stack.name, "Ctrl+Alt+B gives the tool")
         player.clear_cursor()
         EventManager.mock_event(defines.events.on_lua_shortcut, {
            player_index = pindex,
            prototype_name = Rulers.TOOL,
         })
         ctx:assert_equals(Rulers.TOOL, player.cursor_stack.name, "The shortcut gives the tool")
         player.clear_cursor()
      end)
   end)

   it("places rulers with clicks and removes the nearest in line", function(ctx)
      local player, before
      ctx:init(function()
         player = game.get_player(pindex)
         player.clear_cursor()
         before = drawings()
      end)

      ctx:at_tick(1, function()
         EventManager.mock_event(
            "fa-audio-ruler-place",
            { player_index = pindex, cursor_position = { x = 3.5, y = 4.5 } }
         )
         ctx:assert_equals("", listed(Rulers.ruler_tiles(pindex)), "A click without the tool places nothing")

         click("fa-audio-ruler-place", 3.5, 4.5)
         ctx:assert(not player.cursor_stack.valid_for_read, "The click empties the hand, ending the selection")
         click("fa-audio-ruler-place", 10.25, -2.75)
         click("fa-audio-ruler-place", 3.9, 4.1)
         ctx:assert_equals("3 4 10 -3", listed(Rulers.ruler_tiles(pindex)), "Two rulers, no second on one tile")
         ctx:assert_equals(before + 6, drawings(), "Each ruler draws its two lines and its tile")

         click("fa-audio-ruler-remove", 0.5, 0.5)
         ctx:assert_equals("3 4 10 -3", listed(Rulers.ruler_tiles(pindex)), "Off every line removes nothing")

         -- On the first ruler's column, 46 tiles from it.
         click("fa-audio-ruler-remove", 3.5, 50.5)
         ctx:assert_equals("10 -3", listed(Rulers.ruler_tiles(pindex)), "The ruler in line goes")
         ctx:assert_equals(before + 3, drawings(), "Its drawing goes with it")

         click("fa-audio-ruler-remove", -40.5, -2.5)
         ctx:assert_equals("", listed(Rulers.ruler_tiles(pindex)), "The ruler on whose row the click was goes")
         ctx:assert_equals(before, drawings())
      end)

      ctx:at_tick(3, function()
         ctx:assert_equals(Rulers.TOOL, player.cursor_stack.name, "The tool is back in hand a tick later")
         player.clear_cursor()
      end)
   end)
end)
