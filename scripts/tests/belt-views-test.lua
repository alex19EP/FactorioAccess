local TestRegistry = require("scripts.test-registry")
local describe, it = TestRegistry.describe, TestRegistry.it
local BeltAnalyzer = require("scripts.ui.belt-analyzer")

---@param player LuaPlayer
---@param name string
---@param direction defines.direction
---@param extra table? more create_entity parameters
---@return LuaEntity
local function create(player, name, direction, extra)
   local position = player.position --[[@as MapPosition.struct]]
   local spec = {
      name = name,
      position = { math.floor(position.x) + 8, math.floor(position.y) + 8 },
      direction = direction,
      force = player.force,
   }
   for k, v in pairs(extra or {}) do
      spec[k] = v
   end
   local entity = assert(player.surface.create_entity(spec), name .. " not built")
   return entity
end

---@param cell LocalisedString
---@return boolean
local function empty(cell)
   return (cell --[[@as table]])[1] == "fa.ui-belt-analyzer-empty"
end

describe("Belt views", function()
   it("orders a south splitter's lanes west to east", function(ctx)
      ---@type LuaEntity
      local splitter

      ctx:init(function()
         splitter = create(assert(game.get_player(1)), "splitter", defines.direction.south)
      end)

      ctx:at_tick(2, function()
         -- Facing south, the right belt's right lane is the westmost.
         splitter
            .get_transport_line(defines.transport_line.secondary_right_line)
            .insert_at(0.1, { name = "iron-plate" })
         local columns = BeltAnalyzer.splitter_views(splitter)[1].columns
         ctx:assert_equals(4, #columns)
         ctx:assert_equals("fa.ui-belt-analyzer-splitter-lane", columns[1].title[1])
         ctx:assert_equals("West", columns[1].title[2])
         ctx:assert_equals("West", columns[1].title[3])
         ctx:assert_equals("East", columns[4].title[2])
         ctx:assert_equals("East", columns[4].title[3])
         ctx:assert(not empty(columns[1].cells[1]), "the west lane holds the plate")
         for i = 2, 4 do
            ctx:assert(empty(columns[i].cells[1]), "lane " .. i .. " holds nothing")
         end
         splitter.destroy()
      end)
   end)

   it("puts a west belt's north lane first", function(ctx)
      ---@type LuaEntity
      local belt

      ctx:init(function()
         belt = create(assert(game.get_player(1)), "transport-belt", defines.direction.west)
      end)

      ctx:at_tick(2, function()
         local columns = BeltAnalyzer.views(belt)[1].columns
         ctx:assert_equals("North", columns[1].title[2])
         ctx:assert_equals("South", columns[2].title[2])
         belt.destroy()
      end)
   end)

   it("lists what each of a loader's lanes holds", function(ctx)
      ---@type LuaEntity
      local loader

      ctx:init(function()
         loader = create(assert(game.get_player(1)), "loader", defines.direction.north, { type = "output" })
      end)

      ctx:at_tick(2, function()
         loader.get_transport_line(defines.transport_line.right_line).insert_at(0.1, { name = "stone" })
         local columns = BeltAnalyzer.loader_views(loader)[1].columns
         ctx:assert_equals("West", columns[1].title[2])
         ctx:assert(empty(columns[1].cells[1]), "the west lane holds nothing")
         ctx:assert(not empty(columns[2].cells[1]), "the east lane holds the stone")
         loader.destroy()
      end)
   end)
end)
