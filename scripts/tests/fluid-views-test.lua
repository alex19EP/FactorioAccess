local TestRegistry = require("scripts.test-registry")
local describe, it = TestRegistry.describe, TestRegistry.it
local FluidViews = require("scripts.ui.fluid-views")

---Builds three pipes in a row west to east, and a pump north of the first, facing north so that
---it takes from them. Returns the pipes and the pump.
---@param player LuaPlayer
---@return LuaEntity[], LuaEntity
local function build_line(player)
   local x = math.floor(player.position.x) + 4
   local y = math.floor(player.position.y) + 4
   local pipes = {}
   for i = 0, 2 do
      local pipe = player.surface.create_entity({
         name = "pipe",
         position = { x + i + 0.5, y + 0.5 },
         force = player.force,
      })
      assert(pipe, "pipe not built")
      table.insert(pipes, pipe)
   end
   local pump = player.surface.create_entity({
      name = "pump",
      position = { x + 0.5, y - 1 },
      direction = defines.direction.north,
      force = player.force,
   })
   assert(pump, "pump not built")
   return pipes, pump
end

describe("Fluid views", function()
   it("walks the pipeline and counts the buildings it reaches", function(ctx)
      local pipes, pump

      ctx:init(function()
         pipes, pump = build_line(game.get_player(1))
      end)

      ctx:at_tick(2, function()
         local walk = FluidViews.walk_pipeline(pipes[3])
         ctx:assert_equals(3, walk.pipes)
         ctx:assert_equals(true, walk.complete)
         ctx:assert_equals(1, #walk.reached)
         ctx:assert_equals("pump", walk.reached[1].name)
         ctx:assert_equals("input", walk.reached[1].flow)
         ctx:assert_equals(1, walk.reached[1].count)
      end)

      ctx:at_tick(3, function()
         for _, e in ipairs(pipes) do
            e.destroy()
         end
         pump.destroy()
      end)
   end)

   it("names a pipe's shape before its connections", function(ctx)
      local pipes, pump

      ctx:init(function()
         pipes, pump = build_line(game.get_player(1))
      end)

      ctx:at_tick(2, function()
         local cells = FluidViews.connection_cells(pipes[2])
         ctx:assert_equals("fa.ent-info-pipe-horizontal", cells[1][1])
         -- The shape, then one cell per side.
         ctx:assert_equals(5, #cells)
         local pump_cells = FluidViews.connection_cells(pump)
         ctx:assert_equals(2, #pump_cells)
      end)

      ctx:at_tick(3, function()
         for _, e in ipairs(pipes) do
            e.destroy()
         end
         pump.destroy()
      end)
   end)
end)
