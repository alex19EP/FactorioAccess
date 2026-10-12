-- Chapter 5, inserters and defence: the game's second lesson. An abandoned mining site on iron ore,
-- a drill, its chest, an inserter, a stone furnace, a second inserter and the furnace's chest in a
-- column: the first inserter fuelled, a pistol taken from a chest, two biters shot, the second
-- inserter turned the right way and fuelled. Then more drills and furnaces, and plates and coal.
--
-- The map is the campaign's base map (helper-scripts/campaign-base-map): grass and sand, 400 by 400,
-- a pond of water from 16, -14 to 23, -9. This level adds iron, copper, coal and stone patches, the
-- site on the iron and the pistol's chest.

---@diagnostic disable-next-line: unresolved-require
require("story")
local Helpers = require("__FactorioAccess__/campaigns/factorio-access/story-helpers")

local player, carried, no_window, fuelled = Helpers.player, Helpers.carried, Helpers.no_window, Helpers.fuelled
local chapter = Helpers.chapter(5)
local hint_node, goal_node = chapter.hint_node, chapter.goal_node

---@type fa.campaign.Patch[]
local PATCHES = {
   { name = "iron-ore", left = 6, top = -6, width = 10, height = 10 },
   { name = "copper-ore", left = -20, top = -4, width = 8, height = 8 },
   { name = "coal", left = -6, top = 8, width = 8, height = 8 },
   { name = "stone", left = 8, top = 10, width = 6, height = 6 },
}
local ORE_AMOUNT = 2000
-- The site's drill, by its top left tile, on the iron and facing south; the rest of the site stands
-- in a column from where it drops.
local DRILL_TILE = { x = 8, y = -5 }
local PISTOL_CHEST = { x = -3, y = -6 }
-- The game's own tutorial enables these and no others.
local RECIPES = {
   ["iron-plate"] = true,
   ["copper-plate"] = true,
   ["stone-furnace"] = true,
   ["wooden-chest"] = true,
   ["iron-gear-wheel"] = true,
   ["burner-mining-drill"] = true,
   ["transport-belt"] = true,
   ["burner-inserter"] = true,
}
local INSERTER = "burner-inserter"
local BITERS = 2
-- How far north of the character the biters appear, in tiles.
local BITER_DISTANCE = 50
local DRILLS_WANTED = 10
local FURNACES_WANTED = 5
local PLATES_WANTED = 150
local COPPER_WANTED = 50
local COAL_WANTED = 75
-- Stone mined by hand before the hint that a drill could mine it.
local STONE_BEFORE_HINT = 5

---@class (exact) fa.campaign.Site
---@field first fa.Tile the first inserter's tile, between the drill's chest and the furnace
---@field second fa.Tile the second inserter's tile, between the furnace and its chest
---@field output LuaEntity the furnace's chest

---@return fa.campaign.Site
local function site()
   return storage.site
end

---@param tile fa.Tile
---@return LuaEntity?
local function inserter_at(tile)
   return game.surfaces.nauvis.find_entity(INSERTER, { tile.x + 0.5, tile.y + 0.5 })
end

---Whether an inserter of the site's column moves items south, as both should: it faces north, the
---side it picks up from. Its pickup and drop targets would say the same, but an inserter without
---energy has none yet.
---@param inserter LuaEntity
---@return boolean
local function facing_north(inserter)
   return inserter.direction == defines.direction.north
end

---What is wrong with the first inserter, as a goal variant: gone, or turned away from the chest.
---@return string?, any[]?
local function first_wrong()
   local s = site()
   local first = inserter_at(s.first)
   if not first then return "rebuild-first", { s.first.x, s.first.y } end
   if not facing_north(first) then return "turn-first" end
   return nil
end

---@return LuaEntity
local function first()
   return assert(inserter_at(site().first))
end

---@return boolean
local function armed()
   local character = assert(player().character)
   return character.get_inventory(defines.inventory.character_guns).get_item_count("pistol") > 0
      and character.get_inventory(defines.inventory.character_ammo).get_item_count("firearm-magazine") > 0
end

---@return LuaEntity
local function pistol_chest()
   local chest = storage.pistol_chest
   if chest and chest.valid then return chest end
   chest = assert(game.surfaces.nauvis.create_entity({
      name = "iron-chest",
      position = { PISTOL_CHEST.x + 0.5, PISTOL_CHEST.y + 0.5 },
      force = "player",
   }))
   storage.pistol_chest = chest
   return chest
end

local function stock_pistol_chest()
   local chest = pistol_chest()
   chest.insert({ name = "pistol", count = 1 })
   chest.insert({ name = "firearm-magazine", count = 10 })
end

---@return integer
local function biters_left()
   local left = 0
   for _, biter in pairs(storage.biters or {}) do
      if biter.valid then left = left + 1 end
   end
   return left
end

local function send_biters()
   local surface = game.surfaces.nauvis
   local character = assert(player().character)
   local at = character.position
   storage.biters = {}
   for i = 1, BITERS do
      local position = assert(
         surface.find_non_colliding_position("small-biter", { at.x + i * 4 - 6, at.y - BITER_DISTANCE }, 10, 0.5)
      )
      local biter = assert(surface.create_entity({ name = "small-biter", position = position, force = "enemy" }))
      assert(biter.commandable).set_command({
         type = defines.command.attack,
         target = character,
         distraction = defines.distraction.none,
      })
      table.insert(storage.biters, biter)
   end
end

---@param entity LuaEntity
local function empty_burner(entity)
   local burner = assert(entity.burner)
   burner.inventory.clear()
   burner.currently_burning = nil
   entity.energy = 0
end

local function setup()
   local surface = game.surfaces.nauvis
   surface.always_day = true
   surface.peaceful_mode = false
   game.map_settings.pollution.enabled = false
   for _, patch in ipairs(PATCHES) do
      for x = patch.left, patch.left + patch.width - 1 do
         for y = patch.top, patch.top + patch.height - 1 do
            surface.create_entity({ name = patch.name, position = { x + 0.5, y + 0.5 }, amount = ORE_AMOUNT })
         end
      end
   end

   local drill = assert(surface.create_entity({
      name = "burner-mining-drill",
      position = { DRILL_TILE.x + 1, DRILL_TILE.y + 1 },
      direction = defines.direction.south,
      force = "player",
   }))
   drill.insert({ name = "coal", count = 20 })
   local drop = drill.drop_position --[[@as MapPosition.struct]]
   local x, y = math.floor(drop.x), math.floor(drop.y)
   local chest =
      assert(surface.create_entity({ name = "iron-chest", position = { x + 0.5, y + 0.5 }, force = "player" }))
   chest.insert({ name = "iron-ore", count = 10 })
   -- Both inserters face north, the side they pick up from; the second is turned the wrong way.
   local first_inserter = assert(surface.create_entity({
      name = INSERTER,
      position = { x + 0.5, y + 1.5 },
      direction = defines.direction.north,
      force = "player",
   }))
   empty_burner(first_inserter)
   local furnace =
      assert(surface.create_entity({ name = "stone-furnace", position = { x + 1, y + 3 }, force = "player" }))
   furnace.insert({ name = "coal", count = 10 })
   local second_inserter = assert(surface.create_entity({
      name = INSERTER,
      position = { x + 0.5, y + 4.5 },
      direction = defines.direction.south,
      force = "player",
   }))
   empty_burner(second_inserter)
   local output =
      assert(surface.create_entity({ name = "iron-chest", position = { x + 0.5, y + 5.5 }, force = "player" }))
   storage.site = {
      first = { x = x, y = y + 1 },
      second = { x = x, y = y + 4 },
      output = output,
   } --[[@as fa.campaign.Site]]
   stock_pistol_chest()

   local force = game.forces.player
   for name, recipe in pairs(force.recipes) do
      recipe.enabled = RECIPES[name] == true
   end
   force.disable_research()
   player().clear_recipe_notifications()
   local character = assert(player().character)
   for _, inventory in ipairs({
      defines.inventory.character_main,
      defines.inventory.character_guns,
      defines.inventory.character_ammo,
   }) do
      assert(character.get_inventory(inventory)).clear()
   end
   character.insert({ name = "coal", count = 20 })
   force.chart(surface, { { -200, -200 }, { 200, 200 } })
end

---@type fa.campaign.StoryNode[][]
local story_table = {
   {
      {
         init = setup,
         condition = story_elapsed_check(1),
      },
      hint_node("welcome"),
      hint_node("site"),
      hint_node("inserter"),
      goal_node("open", function()
         return player().opened == first()
      end, {
         params = function()
            return { site().first.x, site().first.y }
         end,
         variant = first_wrong,
      }),
      goal_node("fuel", function()
         return fuelled(first())
      end, { variant = first_wrong }),
      goal_node("close", no_window),
      hint_node("inserters"),
      -- Dying in the fight comes back here, by a jump that calls the action of the node before.
      {
         condition = function()
            return true
         end,
      },
      goal_node("pistol", function()
         local chest = pistol_chest()
         return chest.get_inventory(defines.inventory.chest).is_empty() and not player().cursor_stack.valid_for_read
      end),
      goal_node("close-chest", no_window),
      hint_node("weapons"),
      {
         condition = story_elapsed_check(1),
         action = function(_, story)
            if armed() then story_jump_to(story, "footsteps") end
         end,
      },
      goal_node("equip", armed),
      -- The jump past the equipping calls the action of the node before its target, so this one has none.
      {
         condition = function()
            return true
         end,
      },
      hint_node("footsteps"),
      hint_node("shooting"),
      goal_node("fight", function()
         return biters_left() == 0
      end, {
         params = function()
            return { biters_left() }
         end,
         start = function()
            storage.fighting = true
            send_biters()
         end,
         after = function()
            storage.fighting = false
         end,
      }),
      hint_node("after-fight"),
      hint_node("reversed"),
      goal_node("rotate", function()
         local second = inserter_at(site().second)
         return second ~= nil and facing_north(second)
      end, {
         params = function()
            return { site().second.x, site().second.y }
         end,
         variant = function()
            local s = site()
            if not inserter_at(s.second) then return "rebuild-second", { s.second.x, s.second.y } end
            return nil
         end,
      }),
      goal_node("fuel-second", function()
         local second = inserter_at(site().second)
         return second ~= nil and fuelled(second)
      end),
      goal_node("plate", function()
         return site().output.get_item_count("iron-plate") > 0
      end),
      hint_node("turning"),
      hint_node("direct"),
      goal_node("machines", function()
         local force = game.forces.player
         return force.get_entity_count("burner-mining-drill") >= DRILLS_WANTED
            and force.get_entity_count("stone-furnace") >= FURNACES_WANTED
      end, {
         params = function()
            local force = game.forces.player
            return { force.get_entity_count("burner-mining-drill"), force.get_entity_count("stone-furnace") }
         end,
         start = function()
            storage.stone_mined = 0
         end,
      }),
      goal_node("resources", function()
         return carried("iron-plate") >= PLATES_WANTED
            and carried("copper-plate") >= COPPER_WANTED
            and carried("coal") >= COAL_WANTED
      end, {
         params = function()
            return { carried("iron-plate"), carried("copper-plate"), carried("coal") }
         end,
      }),
      hint_node("done"),
      hint_node("end"),
      {
         condition = story_elapsed_check(1),
      },
   },
   -- After dying in the fight: the chest is stocked again, and the pistol is to be taken again.
   {
      hint_node("died"),
      {
         condition = function()
            return true
         end,
         action = function(_, story)
            story_jump_to(story, "pistol")
         end,
      },
   },
}

story_init_helpers(story_table)

script.on_init(function()
   storage.story = story_init()
end)

---@param event EventData
local function update(event)
   -- Nothing to check while the character is dead.
   if player().character then story_update(storage.story, event) end
end

script.on_event(defines.events.on_tick, function(event)
   if not game.tick_paused then update(event) end
end)

-- The goals are checked on the tick too; these events only answer at once.
script.on_event({
   defines.events.on_gui_opened,
   defines.events.on_gui_closed,
   defines.events.on_player_main_inventory_changed,
   defines.events.on_player_cursor_stack_changed,
   defines.events.on_built_entity,
   defines.events.on_player_rotated_entity,
   defines.events.on_entity_died,
}, update)

script.on_event(defines.events.on_player_mined_item, function(event)
   if event.item_stack.name == "stone" and storage.stone_mined then
      storage.stone_mined = storage.stone_mined + event.item_stack.count
      local hinted = storage.stone_hinted --[[@as boolean?]]
      if storage.stone_mined >= STONE_BEFORE_HINT and not hinted then
         storage.stone_hinted = true
         Helpers.hint({ "fa.campaign-05-stone" })
      end
   end
   update(event)
end)

-- In single player the game ends as lost as the character dies (Player::onCharacterDied), before
-- this event. Taken back, the character respawns as in multiplayer, after its respawn time.
script.on_event(defines.events.on_player_died, function()
   game.set_game_state({ game_finished = false })
end)

-- Dying in the fight starts it again from taking the pistol: the biters left go, and the chest gets
-- a new pistol and magazines, the old ones lying in the character's corpse.
script.on_event(defines.events.on_player_respawned, function()
   local fighting = storage.fighting --[[@as boolean?]]
   if not fighting then return end
   storage.fighting = false
   for _, biter in pairs(storage.biters) do
      if biter.valid then biter.destroy() end
   end
   storage.biters = {}
   stock_pistol_chest()
   set_goal("")
   story_jump_to(storage.story, "died")
end)
