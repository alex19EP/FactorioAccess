-- The DLL keeps the cursor on the screen and the remote view's camera on the cursor
-- (native/src/cursor.h); a game without graphics runs no game view, so that is checked in game.
-- These tests cover the mod's side: the screen check for its own moves, the views it speaks, and
-- the map cells it reads.
local TestRegistry = require("scripts.test-registry")
local describe, it = TestRegistry.describe, TestRegistry.it
local Cursor = require("scripts.cursor")
local ViewLimit = require("scripts.view-limit")
local Speech = require("scripts.speech")
local MapCells = require("scripts.map-cells")
local Zoom = require("scripts.zoom")

---Whether a localised string holds `wanted` anywhere inside it.
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

describe("Cursor limited to the screen", function()
   it("allows the tiles on the screen in the character view only", function(ctx)
      ---@type LuaPlayer
      local player

      ctx:init(function()
         player = assert(game.get_player(1))
      end)

      ctx:at_tick(1, function()
         ctx:assert_equals(defines.controllers.character, player.controller_type)
         local position = player.position --[[@as MapPosition.struct]]
         local x, y = math.floor(position.x), math.floor(position.y)
         ctx:assert(ViewLimit.allows(1, { x = x + 2, y = y + 2 }), "A tile next to the character is on the screen")
         ctx:assert(not ViewLimit.allows(1, { x = x + 1000, y = y }), "A tile 1000 tiles away is off the screen")
      end)
   end)

   it("says the view entered, with the ghost in hand", function(ctx)
      ---@type LuaPlayer
      local player

      ctx:init(function()
         player = assert(game.get_player(1))
      end)

      ctx:at_tick(1, function()
         Speech.start_capture()
         player.clear_cursor()
         player.set_controller({ type = defines.controllers.remote, position = player.position })
         player.cursor_ghost = "transport-belt"
      end)

      ctx:at_tick(5, function()
         local said = Speech.stop_capture()
         local found = false
         for _, m in ipairs(said) do
            if contains(m.message, "fa.zoom-view-remote") then
               found = true
               ctx:assert(contains(m.message, "fa.cursor-ghost-description"), "The ghost in hand is said with the view")
            end
         end
         ctx:assert(found, "Entering remote view is said")
         Speech.start_capture()
         player.clear_cursor()
         player.exit_remote_view()
      end)

      ctx:at_tick(9, function()
         local found = false
         for _, m in ipairs(Speech.stop_capture()) do
            if contains(m.message, "fa.zoom-view-character") then found = true end
         end
         ctx:assert(found, "Leaving remote view is said")
      end)
   end)

   it("reads the hand when a ghost is put in it", function(ctx)
      ---@type LuaPlayer
      local player

      ctx:init(function()
         player = assert(game.get_player(1))
      end)

      ctx:at_tick(1, function()
         player.clear_cursor()
         player.set_controller({ type = defines.controllers.remote, position = player.position })
      end)

      ctx:at_tick(5, function()
         Speech.start_capture()
         player.cursor_ghost = "wooden-chest"
      end)

      ctx:at_tick(8, function()
         local found = false
         for _, m in ipairs(Speech.stop_capture()) do
            if contains(m.message, "fa.cursor-ghost-description") then found = true end
         end
         ctx:assert(found, "The ghost put in an empty hand is said")
         player.clear_cursor()
         player.exit_remote_view()
      end)
   end)

   it("reads map cells on the full map", function(ctx)
      ---@type LuaPlayer, number
      local player, old_zoom

      ctx:init(function()
         player = assert(game.get_player(1))
         old_zoom = player.zoom
      end)

      -- A game without graphics never shows the full map, so the cell size is given here.
      ctx:at_tick(1, function()
         player.set_controller({ type = defines.controllers.remote, position = player.physical_position })
         player.zoom = 0.1
      end)

      ctx:at_tick(3, function()
         ctx:assert_equals(8, Zoom.map_cell_size_for(240), "A cell is a thirtieth of the screen")
         ctx:assert_equals(512, Zoom.map_cell_size_for(15360))
         -- 600 tiles across: a thirtieth is 20, the nearest power of two 16
         ctx:assert_equals(600, Zoom.get_current_zoom_tiles(1))
         local size = Zoom.map_cell_size_for(600) --[[@as integer]]
         ctx:assert_equals(16, size)
         Speech.start_capture()
         Cursor.at(1, { x = size / 2, y = size / 2 }, function()
            MapCells.read(1, size)
         end)
         ctx:assert_equals(1, #Speech.stop_capture(), "The cell the cursor is in is read")
         player.zoom = old_zoom
         player.exit_remote_view()
      end)
   end)

   it("limits the cursor in god mode as in the character view", function(ctx)
      ---@type LuaPlayer, LuaEntity?
      local player, character

      ctx:init(function()
         player = assert(game.get_player(1))
         character = player.character
      end)

      -- As the sandbox scenario leaves the player, without a character
      ctx:at_tick(1, function()
         Speech.start_capture()
         player.set_controller({ type = defines.controllers.god })
      end)

      ctx:at_tick(5, function()
         local found = false
         for _, m in ipairs(Speech.stop_capture()) do
            if contains(m.message, "fa.zoom-view-god") then found = true end
         end
         ctx:assert(found, "Entering god mode is said")
         ctx:assert_equals(defines.controllers.god, player.controller_type)
         local position = player.position --[[@as MapPosition.struct]]
         local x, y = math.floor(position.x), math.floor(position.y)
         ctx:assert(ViewLimit.allows(1, { x = x + 2, y = y + 2 }), "A tile next to the god is on the screen")
         ctx:assert(not ViewLimit.allows(1, { x = x + 1000, y = y }), "A tile 1000 tiles away is off the screen")
         player.set_controller({ type = defines.controllers.character, character = character })
      end)
   end)
end)
