-- Chapter 4, mining and crafting: the game's first lesson. A chest capsule's gear wheels, iron ore
-- and stone mined by hand, a stone furnace crafted, built, fed and emptied, then a burner mining
-- drill crafted, put on the quickbar, built on coal and fuelled, and a wooden chest under its drop.
--
-- The map is the campaign's base map (helper-scripts/campaign-base-map): grass and sand, 400 by 400,
-- a pond of water from 16, -14 to 23, -9. This level adds the capsule, an iron and a coal patch, a
-- big rock and a few trees, all near the start.

---@diagnostic disable-next-line: unresolved-require
require("story")
local Helpers = require("__FactorioAccess__/campaigns/factorio-access/story-helpers")

---@type fa.campaign.Patch[]
local PATCHES = {
   { name = "iron-ore", left = 7, top = 2, width = 4, height = 4 },
   { name = "coal", left = -16, top = -6, width = 6, height = 6 },
}
local ORE_AMOUNT = 600
local POD = { x = -5, y = -4 }
local ROCK = { x = -8, y = 6 }
local TREES = {
   { 2, 12 },
   { 4, 13 },
   { 1, 14 },
   { 5, 11 },
}
-- Everything the lesson crafts or smelts; every other recipe is off, as in the game's own tutorial,
-- so that the crafting list is short.
local RECIPES = {
   ["iron-plate"] = true,
   ["stone-furnace"] = true,
   ["iron-gear-wheel"] = true,
   ["burner-mining-drill"] = true,
   ["wooden-chest"] = true,
}
local FURNACE = "stone-furnace"
local DRILL = "burner-mining-drill"
local ENOUGH_ORE = 5
local ENOUGH_STONE = 10
local ENOUGH_COAL = 5
local ENOUGH_PLATES = 5
-- Items the drill puts in its chest before the chest is to be emptied.
local FILLED = 2

local player, carried, built, find = Helpers.player, Helpers.carried, Helpers.built, Helpers.find
local no_window, fuelled = Helpers.no_window, Helpers.fuelled
local chapter = Helpers.chapter(4)
local hint_node, goal_node = chapter.hint_node, chapter.goal_node

---Whether any stone furnace passes `check`.
---@param check fun(furnace: LuaEntity): boolean
---@return boolean
local function any_furnace(check)
   for _, furnace in ipairs(built(FURNACE)) do
      if check(furnace) then return true end
   end
   return false
end

---The chest the drill drops into, if a chest is what it drops into.
---@return LuaEntity?
local function drill_chest()
   local drill = find(DRILL)
   if not drill then return nil end
   local target = drill.drop_target
   if target and target.type == "container" then return target end
   return nil
end

local function setup()
   local surface = game.surfaces.nauvis
   surface.always_day = true
   for _, patch in ipairs(PATCHES) do
      for x = patch.left, patch.left + patch.width - 1 do
         for y = patch.top, patch.top + patch.height - 1 do
            surface.create_entity({ name = patch.name, position = { x + 0.5, y + 0.5 }, amount = ORE_AMOUNT })
         end
      end
   end
   surface.create_entity({ name = "big-rock", position = { ROCK.x + 0.5, ROCK.y + 0.5 } })
   for i, tree in ipairs(TREES) do
      surface.create_entity({ name = string.format("tree-0%d", i), position = { tree[1], tree[2] } })
   end
   local pod = assert(
      surface.create_entity({ name = "crash-site-chest-1", position = { POD.x + 0.5, POD.y + 0.5 }, force = "player" })
   )
   pod.insert({ name = "iron-gear-wheel", count = 2 })
   storage.pod = pod

   local force = game.forces.player
   for name, recipe in pairs(force.recipes) do
      recipe.enabled = RECIPES[name] == true
   end
   force.disable_research()
   player().clear_recipe_notifications()
   assert(player().get_main_inventory()).clear()
   force.chart(surface, { { -200, -200 }, { 200, 200 } })
end

---@return LuaEntity
local function pod()
   return storage.pod
end

---@type fa.campaign.StoryNode[][]
local story_table = {
   {
      {
         init = setup,
         condition = story_elapsed_check(1),
      },
      hint_node("welcome"),
      goal_node("pod", function()
         return not pod().valid or player().opened == pod() or pod().get_item_count() == 0
      end, { params = { { "entity-name.crash-site-chest-1" } } }),
      goal_node("take", function()
         return not pod().valid or pod().get_item_count() == 0
      end),
      goal_node("close", no_window),
      hint_node("backwards"),
      hint_node("mining"),
      goal_node("iron", function()
         return carried("iron-ore") >= ENOUGH_ORE
      end),
      goal_node("stone", function()
         return carried("stone") >= ENOUGH_STONE
      end, { params = { { "entity-name.big-rock" } } }),
      hint_node("crafting"),
      goal_node("furnace", function()
         return carried(FURNACE) > 0 or find(FURNACE) ~= nil
      end),
      hint_node("building"),
      goal_node("build-furnace", function()
         return find(FURNACE) ~= nil
      end),
      goal_node("coal", function()
         return carried("coal") >= ENOUGH_COAL or any_furnace(fuelled)
      end),
      hint_node("windows"),
      goal_node("ore", function()
         return any_furnace(function(furnace)
            return furnace.get_item_count("iron-ore") + furnace.get_item_count("iron-plate") > 0
         end)
      end, { needs = FURNACE }),
      goal_node("fuel", function()
         return any_furnace(fuelled)
      end, { needs = FURNACE }),
      goal_node("plates", function()
         return carried("iron-plate") >= ENOUGH_PLATES or carried(DRILL) > 0
      end, { needs = FURNACE }),
      goal_node("drill", function()
         return carried(DRILL) > 0 or find(DRILL) ~= nil
      end),
      hint_node("quickbar"),
      goal_node("quickbar", function()
         if find(DRILL) then return true end
         local p = player()
         for page = 1, 10 do
            for slot = 1, 10 do
               local filter = p.get_quick_bar_slot(page, slot)
               if filter and filter.filter and filter.filter.name == DRILL then return true end
            end
         end
         return false
      end),
      goal_node("empty-hand", function()
         return not player().cursor_stack.valid_for_read
      end),
      hint_node("drill"),
      goal_node("build-drill", function()
         local drill = find(DRILL)
         return drill ~= nil and drill.mining_target ~= nil
      end, {
         variant = function()
            local drill = find(DRILL)
            if drill and not drill.mining_target then return "drill-off-ore" end
            return nil
         end,
      }),
      goal_node("fuel-drill", function()
         return fuelled(assert(find(DRILL)))
      end, { needs = DRILL }),
      goal_node("wait-drill", function()
         local drill = assert(find(DRILL))
         if drill.drop_target then return true end
         return game.surfaces.nauvis.count_entities_filtered({
            name = "item-on-ground",
            position = drill.drop_position,
            radius = 0.5,
         }) > 0
      end, {
         needs = DRILL,
         after = function(_, story)
            storage.picked_up = false
            if not assert(find(DRILL)).drop_target then return end
            -- Something already catches what it mines, so nothing lies on the ground to pick up.
            story_jump_to(story, "chest")
         end,
      }),
      hint_node("ground"),
      goal_node("pickup", function()
         return storage.picked_up
      end),
      -- The jump past the pickup calls the action of the node before its target, so this one has none.
      {
         condition = function()
            return true
         end,
      },
      goal_node("chest", function()
         return carried("wooden-chest") > 0 or drill_chest() ~= nil
      end),
      goal_node("build-chest", function()
         return drill_chest() ~= nil
      end, {
         needs = DRILL,
         variant = function()
            local drill = find(DRILL)
            if drill and drill.drop_target and not drill_chest() then return "chest-blocked" end
            return nil
         end,
      }),
      goal_node("fill", function()
         local chest = drill_chest()
         return chest ~= nil and chest.get_item_count() >= FILLED
      end, { needs = DRILL }),
      goal_node("empty-chest", function()
         local chest = drill_chest()
         return chest ~= nil and chest.get_item_count() == 0
      end, { needs = DRILL }),
      hint_node("refuel"),
      hint_node("grow"),
      hint_node("packs"),
      hint_node("end"),
      {
         condition = story_elapsed_check(1),
      },
   },
}

story_init_helpers(story_table)

script.on_init(function()
   storage.story = story_init()
end)

script.on_event(defines.events.on_tick, function(event)
   if not game.tick_paused then story_update(storage.story, event, "level-05") end
end)

-- The goals are checked on the tick too; these events only answer at once.
script.on_event({
   defines.events.on_gui_opened,
   defines.events.on_gui_closed,
   defines.events.on_player_main_inventory_changed,
   defines.events.on_player_cursor_stack_changed,
   defines.events.on_built_entity,
   defines.events.on_player_crafted_item,
}, function(event)
   story_update(storage.story, event, "level-05")
end)

-- Picking up an item from the ground: with the pick-up key, or by mining it.
script.on_event(defines.events.on_picked_up_item, function(event)
   storage.picked_up = true
   story_update(storage.story, event, "level-05")
end)

script.on_event(defines.events.on_player_mined_entity, function(event)
   if event.entity.name == "item-on-ground" then storage.picked_up = true end
   story_update(storage.story, event, "level-05")
end)
