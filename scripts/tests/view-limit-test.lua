local TestRegistry = require("scripts.test-registry")
local describe, it = TestRegistry.describe, TestRegistry.it
local Viewpoint = require("scripts.viewpoint")
local ViewLimit = require("scripts.view-limit")
local Speech = require("scripts.speech")
local MapCells = require("scripts.map-cells")
local Zoom = require("scripts.zoom")

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

describe("Cursor limited to the screen", function()
   it("allows the tiles on the screen in the character view only", function(ctx)
      local player

      ctx:init(function()
         player = game.get_player(1)
      end)

      ctx:at_tick(1, function()
         ctx:assert_equals(defines.controllers.character, player.controller_type)
         local x, y = math.floor(player.position.x), math.floor(player.position.y)
         ctx:assert(ViewLimit.allows(1, { x = x + 2, y = y + 2 }), "A tile next to the character is on the screen")
         ctx:assert(not ViewLimit.allows(1, { x = x + 1000, y = y }), "A tile 1000 tiles away is off the screen")
      end)
   end)

   it("opens remote view for a jump off the screen and comes back to the character", function(ctx)
      local player, far

      ctx:init(function()
         player = game.get_player(1)
         far = { x = math.floor(player.position.x) + 400, y = math.floor(player.position.y) }
      end)

      ctx:at_tick(1, function()
         Viewpoint.get_viewpoint(1):set_cursor_pos(far)
         ctx:assert_equals(defines.controllers.remote, player.controller_type, "The jump opens remote view")
         ctx:assert(math.abs(player.position.x - (far.x + 0.5)) < 1, "The camera is on the cursor")
      end)

      ctx:at_tick(2, function()
         local next = { x = far.x + 50, y = far.y }
         Viewpoint.get_viewpoint(1):set_cursor_pos(next)
         ctx:assert(math.abs(player.position.x - (next.x + 0.5)) < 1, "The camera follows the cursor")
         player.exit_remote_view()
      end)

      ctx:at_tick(3, function()
         ctx:assert_equals(defines.controllers.character, player.controller_type)
         local cursor = Viewpoint.get_viewpoint(1):get_cursor_pos()
         ctx:assert_equals(
            math.floor(player.position.x),
            cursor.x,
            "Leaving remote view puts the cursor on the character"
         )
         ctx:assert_equals(math.floor(player.position.y), cursor.y)
      end)
   end)

   it("says the view entered, with the ghost in hand", function(ctx)
      local player

      ctx:init(function()
         player = game.get_player(1)
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

   it("follows a camera the game moves, and brings remote view at the character to the cursor", function(ctx)
      local player, vp, near, far

      ctx:init(function()
         player = game.get_player(1)
         vp = Viewpoint.get_viewpoint(1)
         local x, y = math.floor(player.position.x), math.floor(player.position.y)
         near = { x = x + 5, y = y + 3 }
         far = { x = x + 300, y = y - 200 }
      end)

      -- The game raises the controller change once the handler that made it returns.
      ctx:at_tick(1, function()
         vp:set_cursor_pos(near)
         -- As the map key opens it
         player.set_controller({ type = defines.controllers.remote, position = player.physical_position })
      end)

      ctx:at_tick(2, function()
         ctx:assert_equals(near.x, math.floor(player.position.x), "Remote view at the character comes to the cursor")
         ctx:assert_equals(near.y, math.floor(player.position.y))
         player.exit_remote_view()
      end)

      ctx:at_tick(3, function()
         -- As an alert opens it
         player.set_controller({ type = defines.controllers.remote, position = far })
      end)

      ctx:at_tick(4, function()
         ctx:assert_equals(far.x, vp:get_cursor_pos().x, "Remote view opened elsewhere brings the cursor")
         ctx:assert_equals(far.y, vp:get_cursor_pos().y)
         player.teleport({ far.x + 40.5, far.y + 0.5 })
      end)

      ctx:at_tick(6, function()
         ctx:assert_equals(far.x + 40, vp:get_cursor_pos().x, "The cursor follows a camera the game moved")
         player.exit_remote_view()
      end)
   end)

   it("moves by map cells on the full map", function(ctx)
      local player, vp, old_zoom

      ctx:init(function()
         player = game.get_player(1)
         vp = Viewpoint.get_viewpoint(1)
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
         local size = Zoom.map_cell_size_for(600)
         ctx:assert_equals(16, size)
         local start = vp:get_cursor_pos()
         MapCells.move(1, defines.direction.east, size)
         local cursor = vp:get_cursor_pos()
         ctx:assert_equals(size / 2, cursor.x % size, "The cursor stands at the centre of a cell")
         ctx:assert_equals(size / 2, cursor.y % size)
         ctx:assert_equals(math.floor(start.x / size) + 1, math.floor(cursor.x / size), "It moved one cell east")
         ctx:assert_equals(math.floor(start.y / size), math.floor(cursor.y / size))
         player.zoom = old_zoom
         player.exit_remote_view()
      end)
   end)

   it("pulls an unanchored cursor back onto the screen", function(ctx)
      local player, vp, old_zoom

      ctx:init(function()
         player = game.get_player(1)
         vp = Viewpoint.get_viewpoint(1)
         vp:set_cursor_anchored(false)
         old_zoom = player.zoom
         player.zoom = 1
      end)

      ctx:at_tick(1, function()
         -- At zoom 1 a 1920 pixel wide screen shows 60 tiles across; 20 tiles east is on it.
         local x, y = math.floor(player.position.x), math.floor(player.position.y)
         vp:set_cursor_pos({ x = x + 20, y = y })
         ctx:assert_equals(defines.controllers.character, player.controller_type)
         player.zoom = 3
      end)

      ctx:at_tick(3, function()
         local cursor = vp:get_cursor_pos()
         ctx:assert(ViewLimit.allows(1, cursor), "Zooming in keeps the cursor on the screen")
         ctx:assert(cursor.x > math.floor(player.position.x), "The cursor stays on its side, at the edge")
         player.zoom = old_zoom
      end)
   end)
end)
