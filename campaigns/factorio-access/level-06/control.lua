-- Chapter 6, power plant: the game's third lesson. A boiler, a steam engine and an electric mining
-- drill on coal stand by a lake, unconnected: an offshore pump, pipes for water and steam, fuel and
-- small electric poles get them working. Then a belt from a drill to a furnace, three radars, a
-- second steam engine, and the radars scanning for the crashed ship.
--
-- The map is the campaign's base map (helper-scripts/campaign-base-map): grass and sand, 400 by 400,
-- a pond of water from 16, -14 to 23, -9. This level widens the pond into a lake and adds the plant
-- south of it and iron, copper, coal and stone patches. The game is peaceful: nothing attacks.

---@diagnostic disable-next-line: unresolved-require
require("story")
local Helpers = require("__FactorioAccess__/campaigns/factorio-access/story-helpers")

local player, find, fuelled = Helpers.player, Helpers.find, Helpers.fuelled
local chapter = Helpers.chapter(6)
local hint_node, goal_node = chapter.hint_node, chapter.goal_node

-- The lake, in tiles: left top corner and size. It takes in the base map's pond.
local LAKE = { x = 12, y = -22, width = 18, height = 14 }
---@type fa.campaign.Patch[]
local PATCHES = {
   { name = "coal", left = 32, top = -3, width = 8, height = 9 },
   { name = "iron-ore", left = 26, top = 9, width = 10, height = 8 },
   { name = "copper-ore", left = 6, top = 4, width = 8, height = 8 },
   { name = "stone", left = 0, top = 14, width = 6, height = 6 },
}
local ORE_AMOUNT = 2000
-- The plant by the top left tile of each building. The boiler faces south: water joins it on the
-- west and east of its north row, and steam leaves its south side two tiles above the engine.
local BOILER_TILE = { x = 20, y = -4 }
local ENGINE_TILE = { x = 20, y = 0 }
-- The drill on the coal faces west, toward the plant, and drops into a chest.
local DRILL_TILE = { x = 34, y = 0 }
-- The game's own tutorial enables these at the start, and the rest as the lesson goes.
local RECIPES = {
   ["iron-plate"] = true,
   ["copper-plate"] = true,
   ["stone-furnace"] = true,
   ["iron-stick"] = true,
   ["wooden-chest"] = true,
   ["iron-gear-wheel"] = true,
   ["burner-mining-drill"] = true,
   ["transport-belt"] = true,
   ["burner-inserter"] = true,
   ["pipe"] = true,
   ["pipe-to-ground"] = true,
   ["boiler"] = true,
   ["steam-engine"] = true,
   ["electronic-circuit"] = true,
   ["copper-cable"] = true,
   ["pistol"] = true,
   ["firearm-magazine"] = true,
   ["light-armor"] = true,
   ["offshore-pump"] = true,
}
local RADARS_WANTED = 3
local SECTORS_WANTED = 20

---@class (exact) fa.campaign.Plant
---@field boiler LuaEntity
---@field engine LuaEntity
---@field drill LuaEntity

---@return fa.campaign.Plant
local function plant()
   return storage.plant
end

---@param names string[]
local function enable(names)
   local recipes = game.forces.player.recipes
   for _, name in ipairs(names) do
      recipes[name].enabled = true
   end
end

---@param entity LuaEntity
---@return boolean
local function has_fluid(entity)
   return entity.get_fluid(1) ~= nil
end

---Whether a powered inserter takes from a belt and puts into a furnace that holds something.
---@return boolean
local function belt_feeds_furnace()
   for _, inserter in pairs(game.surfaces.nauvis.find_entities_filtered({ type = "inserter", force = "player" })) do
      local from, to = inserter.pickup_target, inserter.drop_target
      if
         from
         and to
         and from.type == "transport-belt"
         and to.type == "furnace"
         and not assert(to.get_inventory(defines.inventory.crafter_input)).is_empty()
      then
         return true
      end
   end
   return false
end

---While every boiler is out of fuel, a goal that needs power says so instead.
---@return string?
local function out_of_fuel()
   for _, boiler in pairs(Helpers.built("boiler")) do
      if fuelled(boiler) then return nil end
   end
   return "refuel"
end

---How many steam engines with steam each electric network has, by its id. An engine no pole
---reaches is on none.
---@return table<integer, integer>
local function engines_by_network()
   local engines = {}
   for _, engine in pairs(Helpers.built("steam-engine")) do
      local network = engine.electric_network_id --[[@as integer?]]
      if network and has_fluid(engine) then engines[network] = (engines[network] or 0) + 1 end
   end
   return engines
end

---The radars on a network with a running steam engine.
---@return integer
local function radars()
   local engines = engines_by_network()
   local powered = 0
   for _, radar in pairs(Helpers.built("radar")) do
      local network = radar.electric_network_id --[[@as integer?]]
      if network and engines[network] then powered = powered + 1 end
   end
   return powered
end

---The most running steam engines on one network.
---@return integer
local function engines_running()
   local most = 0
   for _, count in pairs(engines_by_network()) do
      most = math.max(most, count)
   end
   return most
end

---@param tile fa.Tile
---@param width integer
---@param height integer
---@return MapPosition
local function centre(tile, width, height)
   return { tile.x + width / 2, tile.y + height / 2 }
end

---@param fields LuaSurface.create_entity_param
---@return LuaEntity
local function fixed(fields)
   fields.force = "player"
   local entity = assert(game.surfaces.nauvis.create_entity(fields))
   entity.minable_flag = false
   entity.destructible = false
   return entity
end

local function setup()
   local surface = game.surfaces.nauvis
   surface.always_day = true
   surface.peaceful_mode = true
   game.map_settings.pollution.enabled = false
   game.map_settings.enemy_expansion.enabled = false

   local water = {}
   for x = LAKE.x, LAKE.x + LAKE.width - 1 do
      for y = LAKE.y, LAKE.y + LAKE.height - 1 do
         water[#water + 1] = { name = "water", position = { x, y } }
      end
   end
   surface.set_tiles(water)
   for _, patch in ipairs(PATCHES) do
      for x = patch.left, patch.left + patch.width - 1 do
         for y = patch.top, patch.top + patch.height - 1 do
            surface.create_entity({ name = patch.name, position = { x + 0.5, y + 0.5 }, amount = ORE_AMOUNT })
         end
      end
   end

   local boiler = fixed({ name = "boiler", position = centre(BOILER_TILE, 3, 2), direction = defines.direction.south })
   local engine =
      fixed({ name = "steam-engine", position = centre(ENGINE_TILE, 3, 5), direction = defines.direction.north })
   local drill = fixed({
      name = "electric-mining-drill",
      position = centre(DRILL_TILE, 3, 3),
      direction = defines.direction.west,
   })
   local drop = drill.drop_position --[[@as MapPosition.struct]]
   surface.create_entity({
      name = "wooden-chest",
      position = { math.floor(drop.x) + 0.5, math.floor(drop.y) + 0.5 },
      force = "player",
   })
   storage.plant = { boiler = boiler, engine = engine, drill = drill } --[[@as fa.campaign.Plant]]

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
   for name, count in pairs({
      ["iron-plate"] = 20,
      ["copper-plate"] = 15,
      ["coal"] = 20,
      ["wood"] = 10,
      ["transport-belt"] = 50,
      ["electric-mining-drill"] = 2,
      ["inserter"] = 10,
      ["small-electric-pole"] = 10,
      ["stone-furnace"] = 10,
      ["pistol"] = 1,
      ["firearm-magazine"] = 5,
   }) do
      character.insert({ name = name, count = count })
   end
   force.chart(surface, { { -200, -200 }, { 200, 200 } })
end

---A hint that unlocks recipes as it shows.
---@param name string
---@param recipes string[]
---@return fa.campaign.StoryNode
local function unlock_node(name, recipes)
   local node = hint_node(name)
   local show = assert(node.action)
   node.action = function(event, story)
      enable(recipes)
      show(event, story)
   end
   return node
end

---@type fa.campaign.StoryNode[][]
local story_table = {
   {
      {
         init = setup,
         condition = story_elapsed_check(1),
      },
      hint_node("welcome"),
      hint_node("ship"),
      hint_node("plant"),
      hint_node("pump"),
      goal_node("pump", function()
         return find("offshore-pump") ~= nil
      end),
      hint_node("pipes"),
      goal_node("water", function()
         return has_fluid(plant().boiler)
      end, { needs = "offshore-pump" }),
      goal_node("fuel", function()
         return fuelled(plant().boiler)
      end),
      goal_node("steam", function()
         return has_fluid(plant().engine)
      end, { needs = "offshore-pump" }),
      hint_node("electricity"),
      hint_node("poles"),
      goal_node("power", function()
         return plant().drill.energy > 0
      end, {
         start = function()
            enable({ "small-electric-pole", "electric-mining-drill", "inserter" })
         end,
         after = function()
            for _, entity in pairs(plant()) do
               entity.minable_flag = true
               entity.destructible = true
            end
            enable({ "assembling-machine-1" })
         end,
      }),
      hint_node("coal"),
      hint_node("belts"),
      goal_node("belt", belt_feeds_furnace, { variant = out_of_fuel }),
      hint_node("radars"),
      goal_node("radars", function()
         return radars() >= RADARS_WANTED
      end, {
         params = function()
            return { radars() }
         end,
         start = function()
            enable({ "radar" })
            storage.counting_radars = true
         end,
         after = function()
            storage.counting_radars = false
         end,
         variant = out_of_fuel,
      }),
      hint_node("engines"),
      goal_node("engine", function()
         return engines_running() >= 2
      end, { variant = out_of_fuel }),
      hint_node("network"),
      unlock_node("turrets", { "gun-turret" }),
      unlock_node("repair", { "repair-pack" }),
      unlock_node("submachine-gun", { "submachine-gun" }),
      goal_node("scan", function()
         return storage.sectors >= SECTORS_WANTED
      end, {
         params = function()
            return { storage.sectors }
         end,
         start = function()
            storage.sectors = 0
         end,
         variant = out_of_fuel,
      }),
      hint_node("scanned"),
      hint_node("wreck"),
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

---@param event EventData
local function update(event)
   if player().character then story_update(storage.story, event, "level-07") end
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
   defines.events.on_player_mined_entity,
}, update)

-- The first two radars each bring a hint.
script.on_event(defines.events.on_built_entity, function(event)
   local counting = storage.counting_radars --[[@as boolean?]]
   if counting and event.entity.name == "radar" then
      local built = game.forces.player.get_entity_count("radar")
      local hinted = storage.radars_hinted or 0
      if built > hinted and built <= 2 then
         storage.radars_hinted = built
         Helpers.hint({ "fa.campaign-06-radar-" .. built })
      end
   end
   update(event)
end)

script.on_event(defines.events.on_sector_scanned, function(event)
   -- Sectors count from the scanning goal on.
   local sectors = storage.sectors --[[@as integer?]]
   if sectors then storage.sectors = sectors + 1 end
   update(event)
end)
