local TestRegistry = require("scripts.test-registry")
local describe, it = TestRegistry.describe, TestRegistry.it
local PoleViews = require("scripts.ui.pole-views")

---Builds two small poles 5 tiles apart west to east, wired together, and an assembling machine
---just west of the first, in its supply area. Returns everything built, the first pole first.
---@param player LuaPlayer
---@return LuaEntity[]
local function build(player)
   local position = player.position --[[@as MapPosition.struct]]
   local x = math.floor(position.x) + 6
   local y = math.floor(position.y) + 6
   ---@param name string
   ---@param at MapPosition
   ---@return LuaEntity
   local function create(name, at)
      local e = player.surface.create_entity({ name = name, position = at, force = player.force })
      assert(e, name .. " not built")
      return e
   end
   local first = create("small-electric-pole", { x + 0.5, y + 0.5 })
   local second = create("small-electric-pole", { x + 5.5, y + 0.5 })
   local machine = create("assembling-machine-1", { x - 1.5, y + 0.5 })
   local id = defines.wire_connector_id.pole_copper
   assert(first.get_wire_connector(id, true)).connect_to(assert(second.get_wire_connector(id, true)), false)
   return { first, second, machine }
end

describe("Pole views", function()
   it("lists what the pole is wired to", function(ctx)
      ---@type LuaEntity[]
      local built

      ctx:init(function()
         built = build(assert(game.get_player(1)))
      end)

      ctx:at_tick(2, function()
         local wired = PoleViews.wired_to(built[1])
         ctx:assert_equals(1, #wired)
         ctx:assert_equals(built[2], wired[1])
         -- The reach, then the one pole.
         ctx:assert_equals(2, #PoleViews.wire_cells(built[1]))
      end)

      ctx:at_tick(3, function()
         for _, e in ipairs(built) do
            e.destroy()
         end
      end)
   end)

   it("counts the electric buildings in the supply area", function(ctx)
      ---@type LuaEntity[]
      local built

      ctx:init(function()
         built = build(assert(game.get_player(1)))
      end)

      ctx:at_tick(2, function()
         local supplied = PoleViews.supplied(built[1])
         ctx:assert_equals(1, #supplied)
         ctx:assert_equals("assembling-machine-1", supplied[1].name)
         ctx:assert_equals("uses", supplied[1].role)
         ctx:assert_equals(1, supplied[1].count)
         ctx:assert_equals(0, #PoleViews.supplied(built[2]))
         ctx:assert_equals(2, #PoleViews.supply_cells(built[2]))
      end)

      ctx:at_tick(3, function()
         for _, e in ipairs(built) do
            e.destroy()
         end
      end)
   end)

   it("attaches the views to the pole's window while it is open", function(ctx)
      ---@type LuaEntity[]
      local built
      ---@type LuaPlayer
      local player

      ctx:init(function()
         player = assert(game.get_player(1))
         built = build(player)
      end)

      ctx:at_tick(2, function()
         player.opened = built[1]
      end)

      ctx:at_tick(3, function()
         local wires = player.gui.relative["fa-entity-view-1"]
         local supply = player.gui.relative["fa-entity-view-2"]
         ctx:assert_not_nil(wires)
         ctx:assert_not_nil(supply)
         ctx:assert_equals("fa.pole-views-wires", wires.caption[1])
         ctx:assert_equals(defines.relative_gui_type.electric_network_gui, wires.anchor.gui)
         -- In the inset panel, one column without a header: a label per cell, the reach then the
         -- one pole.
         ctx:assert_equals(2, #wires.children[1].children[1].children)
         player.opened = nil
      end)

      ctx:at_tick(4, function()
         ctx:assert_nil(player.gui.relative["fa-entity-view-1"])
         ctx:assert_nil(player.gui.relative["fa-entity-view-2"])
         for _, e in ipairs(built) do
            e.destroy()
         end
      end)
   end)
end)
