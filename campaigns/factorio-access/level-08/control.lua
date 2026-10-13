-- Chapter 8, trains: the game's fifth and last lesson, on its own map. The beacon leads to a base
-- biters wrecked, with a railway to a distant iron mine broken in several places. The power plant
-- and the factory come back from ghosts, railway research unlocks rails and trains, the rail
-- planner closes every gap, and a train brings a load of ore from the mine on its own. The car is
-- then packed with what a new base needs.
--
-- The map is the game's own for that lesson (base/campaigns/tutorial/level-05), with its remnants
-- of the wreck. On start, the remnants of the production and power buildings get a ghost of the
-- building on them; walls, turrets, lamps and the rails stay remnants only. Biters are kept, in
-- peaceful mode: they attack only when attacked.
--
-- The map's steam plant has no pole to the base's network, which runs on to the mine, and no
-- remnant marks the gap: a pole ghost there makes it part of the plant to rebuild. The railway
-- ends at each stop, so the train needs a locomotive at each end.

---@diagnostic disable-next-line: unresolved-require
require("story")
local Helpers = require("__FactorioAccess__/campaigns/factorio-access/story-helpers")

local player, built, fuelled = Helpers.player, Helpers.built, Helpers.fuelled
local chapter = Helpers.chapter(8)
local hint_node, goal_node = chapter.hint_node, chapter.goal_node

local SPAWN = { x = -23, y = 5 }
-- The stops, by where the map has them.
local PROCESSING_STOP = { x = -29, y = -7 }
local MINE_STOP = { x = -423, y = -145 }
-- Where remnants become ghosts: the base, the mine and the line between them. The map's far
-- outposts stay as they are.
local GHOST_AREA = { { -480, -220 }, { 140, 90 } }
local POWER_AREA = { { 0, -75 }, { 70, -50 } }
-- Halfway between the plant's last pole and the base's nearest one, 17 tiles apart.
local BRIDGE_POLE = { x = 8.5, y = -54.5 }
-- The buildings whose remnants get a ghost.
local GHOSTED = {
   ["lab"] = true,
   ["assembling-machine-1"] = true,
   ["assembling-machine-2"] = true,
   ["stone-furnace"] = true,
   ["steel-furnace"] = true,
   ["electric-furnace"] = true,
   ["burner-mining-drill"] = true,
   ["electric-mining-drill"] = true,
   ["transport-belt"] = true,
   ["fast-transport-belt"] = true,
   ["underground-belt"] = true,
   ["fast-underground-belt"] = true,
   ["splitter"] = true,
   ["fast-splitter"] = true,
   ["burner-inserter"] = true,
   ["inserter"] = true,
   ["long-handed-inserter"] = true,
   ["fast-inserter"] = true,
   ["small-electric-pole"] = true,
   ["medium-electric-pole"] = true,
   ["big-electric-pole"] = true,
   ["steam-engine"] = true,
   ["boiler"] = true,
   ["offshore-pump"] = true,
   ["pipe"] = true,
   ["pipe-to-ground"] = true,
   ["solar-panel"] = true,
   ["accumulator"] = true,
   ["wooden-chest"] = true,
   ["iron-chest"] = true,
   ["steel-chest"] = true,
}
local POWER_PLANT = { "steam-engine", "boiler", "offshore-pump", "pipe", "pipe-to-ground", "medium-electric-pole" }
-- The game's own lesson starts with these technologies researched, and these to research.
local RESEARCHED = {
   ["electronics"] = true,
   ["steam-power"] = true,
   ["automation-science-pack"] = true,
   ["electric-mining-drill"] = true,
   ["automation-2"] = true,
   ["automation"] = true,
   ["automobilism"] = true,
   ["engine"] = true,
   ["fast-inserter"] = true,
   ["heavy-armor"] = true,
   ["logistic-science-pack"] = true,
   ["logistics-2"] = true,
   ["logistics"] = true,
   ["military"] = true,
   ["radar"] = true,
   ["repair-pack"] = true,
   ["lamp"] = true,
   ["physical-projectile-damage-1"] = true,
   ["steel-axe"] = true,
   ["steel-processing"] = true,
   ["stone-wall"] = true,
   ["gun-turret"] = true,
   ["weapon-shooting-speed-1"] = true,
}
local AVAILABLE = {
   ["advanced-material-processing"] = true,
   ["automated-rail-transportation"] = true,
   ["concrete"] = true,
   ["electric-energy-distribution-1"] = true,
   ["gate"] = true,
   ["landfill"] = true,
   ["military-2"] = true,
   ["physical-projectile-damage-2"] = true,
   ["railway"] = true,
   ["research-speed-1"] = true,
   ["research-speed-2"] = true,
   ["solar-energy"] = true,
   ["stronger-explosives-1"] = true,
   ["toolbelt"] = true,
   ["weapon-shooting-speed-2"] = true,
}
local ITEMS = {
   ["assembling-machine-1"] = 10,
   ["coal"] = 40,
   ["copper-plate"] = 100,
   ["electric-mining-drill"] = 5,
   ["electronic-circuit"] = 100,
   ["submachine-gun"] = 1,
   ["firearm-magazine"] = 40,
   ["shotgun"] = 1,
   ["shotgun-shell"] = 20,
   ["heavy-armor"] = 1,
   ["inserter"] = 20,
   ["iron-gear-wheel"] = 50,
   ["iron-plate"] = 200,
   ["lab"] = 5,
   ["long-handed-inserter"] = 10,
   ["pipe"] = 40,
   ["small-electric-pole"] = 20,
   ["small-lamp"] = 20,
   ["stone"] = 200,
   ["transport-belt"] = 50,
   ["wood"] = 50,
}
-- What chapter 7 packed: the car arrives with it.
local CARGO = {
   ["iron-plate"] = 400,
   ["copper-plate"] = 200,
   ["gun-turret"] = 20,
   ["firearm-magazine"] = 200,
   ["steel-plate"] = 50,
   ["coal"] = 100,
}
-- What the car takes on to a new base, in this order.
local SUPPLIES = {
   { name = "piercing-rounds-magazine", count = 200 },
   { name = "steel-plate", count = 200 },
   { name = "iron-gear-wheel", count = 250 },
   { name = "electronic-circuit", count = 500 },
   { name = "solar-panel", count = 200 },
}
-- An underground belt's remnant has one of 8 directions, for the belt's direction and whether it
-- was the entrance or the exit (found by destroying each kind in game).
---@type table<defines.direction, {direction: defines.direction, type: "input"|"output"}>
local UNDERGROUND_REMNANTS = {
   [defines.direction.south] = { direction = defines.direction.north, type = "input" },
   [defines.direction.east] = { direction = defines.direction.north, type = "output" },
   [defines.direction.southwest] = { direction = defines.direction.east, type = "input" },
   [defines.direction.southeast] = { direction = defines.direction.east, type = "output" },
   [defines.direction.west] = { direction = defines.direction.south, type = "input" },
   [defines.direction.north] = { direction = defines.direction.south, type = "output" },
   [defines.direction.northwest] = { direction = defines.direction.west, type = "input" },
   [defines.direction.northeast] = { direction = defines.direction.west, type = "output" },
}

---@class (exact) fa.campaign.Stops
---@field processing LuaEntity
---@field mine LuaEntity

---@return fa.campaign.Stops
local function stops()
   return storage.stops
end

---@param name string
---@return boolean
local function researched(name)
   return game.forces.player.technologies[name].researched
end

---@param area BoundingBox
---@param names string[]
---@return integer
local function ghosts_left(area, names)
   return game.surfaces.nauvis.count_entities_filtered({ area = area, type = "entity-ghost", ghost_name = names })
end

---While the power plant's boilers are out of fuel, a goal that needs power says so instead.
---@return string?
local function out_of_fuel()
   local boilers = game.surfaces.nauvis.find_entities_filtered({ area = POWER_AREA, name = "boiler", force = "player" })
   for _, boiler in pairs(boilers) do
      if fuelled(boiler) then return nil end
   end
   return "refuel"
end

---Whether a train can go from `from` to `to`, leaving `from` either way: the line ends at each
---stop, and a train with a locomotive at each end drives off backwards.
---@param from LuaEntity
---@param to LuaEntity
---@return boolean
local function rail_path(from, to)
   local rail = from.connected_rail
   if not (rail and to.connected_rail) then return false end
   local result = game.train_manager.request_train_path({
      goals = { { train_stop = to } },
      starts = {
         { rail = rail, direction = defines.rail_direction.front },
         { rail = rail, direction = defines.rail_direction.back },
      },
   }) --[[@as TrainPathFinderOneGoalResult]]
   return result.found_path
end

---@return boolean
local function railway_joined()
   return rail_path(stops().processing, stops().mine) and rail_path(stops().mine, stops().processing)
end

---A train with a cargo wagon and a locomotive facing each way.
---@return LuaTrain?
local function train()
   for _, locomotive in pairs(built("locomotive")) do
      local found = locomotive.train
      local movers = found.locomotives
      if #found.cargo_wagons > 0 and #movers.front_movers > 0 and #movers.back_movers > 0 then return found end
   end
   return nil
end

---@return boolean
local function train_fuelled()
   local found = train()
   if not found then return false end
   -- Each pulls only its way: one without fuel leaves the train stuck at the far stop.
   for _, movers in pairs(found.locomotives) do
      for _, locomotive in pairs(movers) do
         if not fuelled(locomotive) then return false end
      end
   end
   return true
end

---Whether a train driving itself brought ore to the processing stop and is being unloaded there.
---@return boolean
local function delivered()
   local arrival = storage.arrival --[[@as {train: integer, ore: integer}?]]
   if not (arrival and arrival.ore > 0) then return false end
   local arrived = game.train_manager.get_train_by_id(arrival.train)
   return arrived ~= nil and arrived.station == stops().processing and arrived.get_item_count("iron-ore") < arrival.ore
end

---What the player's cars hold in their trunks and gun slots: the one they came in, and any they
---built or drove. A car in the map's far outposts counts once the player gets in.
---@return table<string, integer>
local function packed()
   local counts = {}
   for _, car in pairs(storage.cars) do
      -- A car picked up is gone from the count.
      if car.valid then
         for _, inventory in ipairs({ defines.inventory.car_trunk, defines.inventory.car_ammo }) do
            for _, item in pairs(assert(car.get_inventory(inventory)).get_contents()) do
               counts[item.name] = (counts[item.name] or 0) + item.count
            end
         end
      end
   end
   return counts
end

---What the car still lacks, as a list for the goal; nil once it has everything.
---@return LocalisedString?
local function missing()
   local counts = packed()
   ---@type LocalisedString
   local list = { "" }
   for _, wanted in ipairs(SUPPLIES) do
      local lacking = wanted.count - (counts[wanted.name] or 0)
      if lacking > 0 then
         if #list > 1 then list[#list + 1] = ", " end
         list[#list + 1] = { "fa.campaign-08-item", lacking, prototypes.item[wanted.name].localised_name }
      end
   end
   return #list > 1 and list or nil
end

local function ghost_the_wreck()
   local surface = game.surfaces.nauvis
   for _, corpse in pairs(surface.find_entities_filtered({ type = { "corpse", "rail-remnants" } })) do
      if corpse.name:find("remnants", 1, true) then corpse.corpse_expires = false end
   end
   for _, corpse in pairs(surface.find_entities_filtered({ type = "corpse", area = GHOST_AREA })) do
      local name = corpse.name:match("^(.*)%-remnants$")
      if name and GHOSTED[name] then
         ---@type LuaSurface.create_entity_param
         local fields = {
            name = "entity-ghost",
            inner_name = name,
            position = corpse.position,
            direction = corpse.direction,
            force = "player",
         }
         if prototypes.entity[name].type == "underground-belt" then
            local was = UNDERGROUND_REMNANTS[corpse.direction]
            fields.direction, fields.type = was.direction, was.type
         end
         -- Something rebuilt on the spot keeps it.
         surface.create_entity(fields)
      end
   end
end

local function setup()
   local surface = game.surfaces.nauvis
   surface.always_day = true
   surface.peaceful_mode = true
   game.map_settings.pollution.enabled = false
   game.map_settings.enemy_expansion.enabled = false
   game.map_settings.enemy_evolution.enabled = false
   game.forces.enemy.set_evolution_factor(0, surface)

   ghost_the_wreck()
   assert(surface.create_entity({
      name = "entity-ghost",
      inner_name = "medium-electric-pole",
      position = BRIDGE_POLE,
      force = "player",
   }))
   storage.stops = {
      processing = assert(surface.find_entity("train-stop", PROCESSING_STOP)),
      mine = assert(surface.find_entity("train-stop", MINE_STOP)),
   } --[[@as fa.campaign.Stops]]

   local force = game.forces.player
   for name, technology in pairs(force.technologies) do
      technology.researched = RESEARCHED[name] == true
      technology.enabled = RESEARCHED[name] == true or AVAILABLE[name] == true
   end
   force.reset_technology_effects()
   player().clear_recipe_notifications()
   local character = assert(player().character)
   for _, inventory in ipairs({
      defines.inventory.character_main,
      defines.inventory.character_guns,
      defines.inventory.character_ammo,
   }) do
      assert(character.get_inventory(inventory)).clear()
   end
   for name, count in pairs(ITEMS) do
      character.insert({ name = name, count = count })
   end

   local position = assert(surface.find_non_colliding_position("car", { SPAWN.x - 3, SPAWN.y }, 10, 0.5))
   local car = assert(surface.create_entity({ name = "car", position = position, force = "player" }))
   assert(car.get_inventory(defines.inventory.fuel)).insert({ name = "coal", count = 10 })
   local trunk = assert(car.get_inventory(defines.inventory.car_trunk))
   for name, count in pairs(CARGO) do
      trunk.insert({ name = name, count = count })
   end
   storage.cars = { [assert(car.unit_number)] = car } --[[@as table<integer, LuaEntity>]]

   force.chart(surface, { { MINE_STOP.x - 60, MINE_STOP.y - 80 }, { SPAWN.x + 160, SPAWN.y + 100 } })
end

---@param goal string
---@param technology string
---@return fa.campaign.StoryNode
local function research_goal(goal, technology)
   return goal_node(goal, function()
      return researched(technology)
   end, { variant = out_of_fuel })
end

-- The game's last screen: the lessons are over, and the map stays to play on.
local function finish()
   game.set_win_ending_info({
      title = { "fa.campaign-08-final-title" },
      message = { "fa.campaign-08-final-text" },
   })
   game.set_game_state({ game_finished = true, player_won = true, can_continue = true })
   return true
end

---@type fa.campaign.StoryNode[][]
local story_table = {
   {
      {
         init = setup,
         condition = story_elapsed_check(1),
      },
      hint_node("welcome"),
      hint_node("car"),
      hint_node("wreck"),
      hint_node("rebuild"),
      goal_node("power", function()
         return ghosts_left(POWER_AREA, POWER_PLANT) == 0
      end, {
         params = function()
            return { ghosts_left(POWER_AREA, POWER_PLANT) }
         end,
      }),
      hint_node("science"),
      research_goal("railway-research", "railway"),
      hint_node("rails"),
      hint_node("line"),
      hint_node("planner"),
      goal_node("railway", railway_joined),
      research_goal("automated", "automated-rail-transportation"),
      hint_node("rolling-stock"),
      hint_node("place-train"),
      goal_node("train", function()
         return train() ~= nil
      end),
      goal_node("fuel", train_fuelled, { needs = "locomotive" }),
      hint_node("schedule"),
      goal_node("delivery", delivered, {
         needs = "locomotive",
         start = function()
            storage.arrival = nil
         end,
      }),
      hint_node("delivered"),
      hint_node("supplies-1"),
      hint_node("supplies-2"),
      goal_node("supplies", function()
         return missing() == nil
      end, {
         params = function()
            return { missing() or "" }
         end,
         needs = "car",
      }),
      hint_node("ready"),
      hint_node("end"),
      {
         condition = story_elapsed_check(1),
      },
   },
}

story_init_helpers(story_table)

script.on_init(function()
   storage.story = story_init()
   game.forces.player.set_spawn_position(SPAWN, game.surfaces.nauvis)
end)

---@param event EventData
local function update(event)
   if player().character then story_update(storage.story, event, nil, finish) end
end

script.on_event(defines.events.on_tick, function(event)
   if game.tick_paused then return end
   update(event)
end)

-- A train driving itself that stops at the processing stop: what ore it brought.
script.on_event(defines.events.on_train_changed_state, function(event)
   local arrived = event.train
   if
      arrived.state == defines.train_state.wait_station
      and not arrived.manual_mode
      and storage.stops
      and arrived.station == stops().processing
   then
      storage.arrival = { train = arrived.id, ore = arrived.get_item_count("iron-ore") }
   end
   update(event)
end)

-- The goals are checked on the tick too; these events only answer at once.
script.on_event({
   defines.events.on_gui_opened,
   defines.events.on_gui_closed,
   defines.events.on_player_main_inventory_changed,
   defines.events.on_player_cursor_stack_changed,
   defines.events.on_player_mined_entity,
   defines.events.on_research_finished,
}, update)

---@param car LuaEntity?
local function keep_car(car)
   if car and car.valid and car.type == "car" then storage.cars[assert(car.unit_number)] = car end
end

script.on_event(defines.events.on_built_entity, function(event)
   keep_car(event.entity)
   update(event)
end)

script.on_event(defines.events.on_player_driving_changed_state, function(event)
   keep_car(event.entity)
end)
