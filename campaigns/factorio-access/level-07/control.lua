-- Chapter 7, research: the game's fourth lesson, at its full cost. A lab and the first research,
-- then the factory that research needs: a belt of coal for the boiler, assemblers making science
-- packs, belt-fed smelting columns, underground belts and splitters, and green packs. Automobilism
-- gives a car, driven 200 tiles and packed for the trip. Old example blueprints come with the
-- topics they show, and with them drag building, rulers, copy and paste.
--
-- The map is the campaign's base map (helper-scripts/campaign-base-map), with chapter 6's lake.
-- The plant stands running south of it: a pump, a boiler and two steam engines, an electric drill
-- on coal, and drills on iron and copper dropping into furnaces, all on poles. The game is
-- peaceful: nothing attacks.

---@diagnostic disable-next-line: unresolved-require
require("story")
local Helpers = require("__FactorioAccess__/campaigns/factorio-access/story-helpers")

local player, find, built, fuelled = Helpers.player, Helpers.find, Helpers.built, Helpers.fuelled
local chapter = Helpers.chapter(7)
local hint_node, goal_node = chapter.hint_node, chapter.goal_node

-- The lake, in tiles: left top corner and size, as chapter 6 has it.
local LAKE = { x = 12, y = -22, width = 18, height = 14 }
---@type fa.campaign.Patch[]
local PATCHES = {
   { name = "coal", left = 32, top = -6, width = 12, height = 12 },
   { name = "iron-ore", left = 26, top = 10, width = 20, height = 16 },
   { name = "copper-ore", left = 2, top = 10, width = 12, height = 12 },
   { name = "stone", left = -8, top = 10, width = 6, height = 6 },
}
local ORE_AMOUNT = 5000
-- The pump on the shore, then pipes south to the boiler's west water connection.
local PUMP = { x = 19.5, y = -7.5 }
local PIPE_TILES = { { x = 19, y = -7 }, { x = 19, y = -6 }, { x = 19, y = -5 }, { x = 19, y = -4 } }
-- The plant by the top left tile of each building: the boiler faces south, and the engines stand
-- end to end below it.
local BOILER_TILE = { x = 20, y = -4 }
local ENGINE_TILES = { { x = 20, y = -2 }, { x = 20, y = 3 } }
-- The coal drill faces west, dropping onto the ground for the player's belt.
local COAL_DRILL_TILE = { x = 34, y = 0 }
-- Drills facing north, each dropping into the furnace north of it.
---@type fa.Tile[]
local IRON_DRILL_TILES = { { x = 26, y = 11 }, { x = 29, y = 11 }, { x = 32, y = 11 }, { x = 35, y = 11 } }
---@type fa.Tile[]
local COPPER_DRILL_TILES = { { x = 4, y = 11 }, { x = 7, y = 11 } }
local POLE_TILES = {
   { x = 19, y = 0 },
   { x = 19, y = 5 },
   { x = 26, y = 0 },
   { x = 32, y = -1 },
   { x = 24, y = 8 },
   { x = 28, y = 10 },
   { x = 34, y = 10 },
   { x = 16, y = 7 },
   { x = 11, y = 9 },
   { x = 6, y = 10 },
}
-- The game's own lesson starts with these technologies researched, and these to research.
local RESEARCHED = {
   ["electronics"] = true,
   ["steam-power"] = true,
   ["automation-science-pack"] = true,
   ["electric-mining-drill"] = true,
   ["automation"] = true,
   ["gun-turret"] = true,
   ["military"] = true,
   ["radar"] = true,
   ["repair-pack"] = true,
   ["lamp"] = true,
}
local AVAILABLE = {
   ["automation-2"] = true,
   ["automobilism"] = true,
   ["engine"] = true,
   ["fast-inserter"] = true,
   ["heavy-armor"] = true,
   ["logistic-science-pack"] = true,
   ["logistics-2"] = true,
   ["logistics"] = true,
   ["physical-projectile-damage-1"] = true,
   ["steel-axe"] = true,
   ["steel-processing"] = true,
   ["stone-wall"] = true,
   ["weapon-shooting-speed-1"] = true,
}
local ITEMS = {
   ["iron-plate"] = 10,
   ["copper-plate"] = 10,
   ["coal"] = 40,
   ["transport-belt"] = 100,
   ["inserter"] = 20,
   ["small-electric-pole"] = 20,
   ["electric-mining-drill"] = 5,
   ["pistol"] = 1,
   ["firearm-magazine"] = 20,
   ["electronic-circuit"] = 40,
}
-- What the car takes on the trip, in this order.
local PACKING = {
   { name = "iron-plate", count = 400 },
   { name = "copper-plate", count = 200 },
   { name = "gun-turret", count = 20 },
   { name = "firearm-magazine", count = 200 },
   { name = "steel-plate", count = 50 },
   { name = "coal", count = 100 },
}
local LABS_WANTED = 3
local FURNACES_WANTED = 8
local DRIVE_WANTED = 200

-- The old tutorial's example blueprints.
local BLUEPRINTS = {
   science = "0eNqVk2GPgjAMhv8K6efNyAQV/ooxZsxGmxuF2+A8Y/jvt4me5kLM+Ymsa5+3b1cuUNkeW0fcQXkBMg17KDcX8HRgbWOMdY1QgvYe68oSH2StzZEYZQqDAOI9fkOZDmKiyOrqKUcNWwHIHXWEo8r1cN5xX1foAkTcC32trZVo0XSOjGwbiyCgbXyobTgqBJ7MlrNcwBnK1SwfYgN/eOpdnnrNW4iX05gArm/A8A1zcGiojeXkGpYH1E6ejogW7le7z17boBhSuHGhZ5joIvvtgtij60LsxWhG5T0FgfE+VRPM/G1n2bQz3XdNrWOm9IaQDcpWm4/3HC7/5VC95XAlnpdyAjd/wsVFPQVg3NJNKnKhRL4NMeqwDoTHPyMiLTxgCSp5TC65Tc4nmveJTkbFL3T+KpgvVZEVRb5OF0Uxz4bhBw9XJwU=",
   power = "0eNqV0s1ugzAMAOBXQT6HakBCCa9SVVOgVmUphIqk2yqUd58BrT2MreWWWPbn/HiExl7xMpALUI9Abe881IcRPJ2dsVPMmQ6hBh/QdCm6MzmEKIDcCb+gzuJRALpAgXCpnDe3d3ftGhw4QawKAi6956LeTT0YSnW5UwJuvNrvVIzil5S/LBVPpOIhdcbaFC22YaA2vfR2Fax+QLkOyq1g9gRU218tX5fK7a/2h7S/S01PliP/nCZbA6oXgOIB8GB90jBP1aEQSkihjhyjgN1E3OdWgDUN8qxCniywF4lM5usmy3U5YNwpmX7Dc/4Hp8xNVZlrqbWqskLrNxnjN22M/Ek=",
   smelter = "0eNqd1O1uwiAUBuBbMec3GKGtlt6KWRZazxwJhQZw0Zje+2hddIk01v7rBzwHXj6uUOsTdk6ZANUVVGONh2p/Ba+ORurhm5EtQgXBSeM76wKtUQfoCShzwDNUrP8ggCaooPDWd3y5fJpTW6OLDciEQaCzPnazZqgTKSrEuiBwiU/Zuuh78mTxJRZPW9kbVvnCypdYE3Ms7pa25ki/Zcz5QJXx6EL8nxB3/0UCB+WwuTVgPFFgey8wC+Wz0N0sNH8PLe+oD9Yg/To5IxtMydu/BFKJijdWJ5vMskzIbLOE5rPoReeGpfcU40s26BT2ODm+lVpT1HEiTjW0szq9OK/EfEmM7DnGeBWpgG10HvcaAS2jNdhKo6w1rnwbbXQrPMu2G4f8g86PRrHlIheiKFkmxCbv+1/VxKuF",
   drills = "0eNqdlMtuwyAQRX8FzRpHxo/48StVVZEwSkbC2AJSNYr878Wuki6Ko4adGcE5w/iKGxz0BSdLxkN/AzqOxkH/dgNHJyP1UjNyQOgBNR69pWM2kCFzypQlrWHmQEbhF/RifueAxpMn/EGsi+uHuQwHtGEDf47iMI0unB7NYg3ErM13NYdr+Cp2dTApsuHguqGa+R9B8RB4K42bRuuzA2ofITftS+TyBXJzJ5eBHGFVKawizqpfHmlTb15cFBHD/mFwg9Q6e3imUePTwYp4y03K9TdYbQorj7O69HTm/4mQyBPS+T+ySBjDxhREkcDa+DuiTE9nHglneF7I4xBwv08WBy1De6FWsRXq2MUFA1t6Zo4U6lGqpeBHdg5LJg27d8OWDDMyzJ+RDaTUmuhPtG5V1vuiq7qubkXZdXk1z99Mr7Gw",
   -- The belt examples go in a book.
   belts = {
      "0eNqdkttugzAMhl8F+TpUHKeFV5mmKUBELYGDcliHEO8+h16s2tpp7CrJH/v7bScrtGPQs0Xy0KyAnSEHzcsKDgdSY9RITRoa8FaRm431aatHD5sApF5/QJNv4k544Es7WMPr94RiexWgyaNHfTXbD8sbhanVloniIUXAbBwnGopeDEvzLDvVAhbelqeaXfwyx1QT/Byi7Q98IR709Cu82OE9Wt1dI6o76PIIOj+Erv6OlvIQuf7nuPPbcSPt0+aXRa8nFr7+lYBRMY01rZy/KNsnEZ5c0J8T4j7O6WCQhuTGPumscS6KGAHv2rq9iPqpkJWU9XNeSplV2/YJCVPpZA==",
      "0eNqd0ttqwzAMBuBXKbpWS5zEZc6rlFGSVhQNWwm2M1qC331ucrHBUthyZxv9n+XDBJ0dafAsEZoJ+NJLgOY0QeCbtPa5Jq0jaCD6VsLQ+7jvyEZICCxXukOj0jsCSeTItGTnyeMso+vI5wJ8YSAMfcixXp77ZGqviuKgER55WB10SvgLKzdh5TpW/QdTPzGEK3u6LBX1Cl3/nTbmpazKFVpv6lqtX8FxS5+zlV+dI7mc/P5CCLbN6bwW2A2Wdo78jXYfoywHQvgkH2ZTH0tTG6PfVGVMUaf0BYIW1Ew=",
      "0eNqdkmFrwyAQhv+K3Gdbqolj5q+UUUwn3YE5g17XleB/n0kZG1vGWL/pcfe87/k6QR/OfkxIDN0EeIyUodtPkPFELsw1coOHDjg5ymNMvOl9YCgSkJ79G3SqyJX2PAZk9ulLoy5PEjwxMvqbyHK5Hug89LWzU/IXMQljzHUs0qxQURu101sj4VqPzdaU2cI3mJY/rKxibhC9Dmn+40j94ai9az21DjN3OVtgNQVkP9TRz+wlBFfHa82Jj/cSF+QXMUOziCRcCKIV9YOQPzLSSTCGmqOEV5/yImcetG2tNY+qsXbXlvIOTGbGmg==",
      "0eNqd0c1qwzAMAOBXCTq7I87PmPMqZQyn1obAkYPtjJbgd5/SMnZYemhvspA+S2iF0S84R+IMwwp0CpxgOK6Q6Iut33JsJ4QBcrSc5hDzYUSfoSggdniGQZd3BciZMuGt9/q4fPAyjRilQN0xFMwhSVvg7R+hDrpuXnoFFwklKEX9w5pHsPYX04IpcBTxdKvoduj2qTn1/pzdU1i9j/WPYPru0rrZTkUZJ5H+7q7AW9Ekh2c7zR6r8FnZyoVllDiRQx+sk7JvjOkK9a+N6Yzp33RrTN2V8gOiP73s",
      "0eNqdkM1qwzAQhF9F7FkJ8V+p/SqhlLW1aRfklZGUkGD07l0nhxbaHtrb7jDzDcwKoz/TElkyDCvwFCTBcFwh8Zug3zTBmWCAHFHSEmLejeQzFAssjq4wVOXFAknmzPTI3p/bq5znkaIa7C8MC0tIGguy9ShqVx2afWfhpme977TDcaTp4WiL/Yau/4Kuv6J/gDX/glUbTBfgTLNGP+e04FHjqtEV58WTCSeDYlgu6NmZKUShaPI7ZnMKcU4GTWJHPqBTU8qETiEXiune3D3Vfdv33XPV9P2hLeUDeMeSxg==",
   },
}

---@class (exact) fa.campaign.CoalPlant
---@field boiler LuaEntity
---@field drill LuaEntity

---@return fa.campaign.CoalPlant
local function plant()
   return storage.coal_plant
end

---@param type string
---@return LuaEntity[]
local function built_type(type)
   return game.surfaces.nauvis.find_entities_filtered({ type = type, force = "player" })
end

---@param entity LuaEntity
---@return string?
local function recipe_of(entity)
   local recipe = entity.get_recipe()
   return recipe and recipe.name
end

---What the working inserters take from, by the unit number of what they put into.
---@return table<integer, LuaEntity[]>
local function feeds()
   local by_target = {}
   for _, inserter in pairs(built_type("inserter")) do
      local from, to = inserter.pickup_target, inserter.drop_target
      if from and to and to.unit_number then
         local list = by_target[to.unit_number] or {}
         list[#list + 1] = from
         by_target[to.unit_number] = list
      end
   end
   return by_target
end

---Whether an inserter puts into `target` from something `test` accepts.
---@param fed table<integer, LuaEntity[]>
---@param target LuaEntity
---@param test fun(from: LuaEntity): boolean
---@return boolean
local function fed_from(fed, target, test)
   for _, from in ipairs(fed[target.unit_number] or {}) do
      if test(from) then return true end
   end
   return false
end

---@param entity LuaEntity
---@return boolean
local function is_belt(entity)
   return entity.type == "transport-belt"
end

---@param recipe string
---@return fun(entity: LuaEntity): boolean
local function machine_making(recipe)
   return function(entity)
      return entity.type == "assembling-machine" and recipe_of(entity) == recipe
   end
end

---The labs fed science packs of `pack` straight from an assembler that has made some. Red packs
---count only from an assembler fed gears from another assembler.
---@param pack string
---@return integer
local function fed_labs(pack)
   local fed = feeds()
   local makes_gears = machine_making("iron-gear-wheel")
   local makes_packs = machine_making(pack)
   local count = 0
   for _, lab in pairs(built("lab")) do
      if
         fed_from(fed, lab, function(machine)
            return makes_packs(machine)
               and machine.products_finished > 0
               and (pack ~= "automation-science-pack" or fed_from(fed, machine, makes_gears))
         end)
      then
         count = count + 1
      end
   end
   return count
end

---The furnaces fed from a belt that have smelted something.
---@return integer
local function belt_furnaces()
   local fed = feeds()
   local count = 0
   for _, furnace in pairs(built_type("furnace")) do
      if furnace.products_finished > 0 and fed_from(fed, furnace, is_belt) then count = count + 1 end
   end
   return count
end

---@return boolean
local function boiler_on_belt()
   local belt = plant().drill.drop_target
   return belt ~= nil and is_belt(belt) and fed_from(feeds(), plant().boiler, is_belt)
end

---@return boolean
local function gears_from_belt()
   local fed = feeds()
   for _, machine in pairs(built_type("assembling-machine")) do
      if recipe_of(machine) == "iron-gear-wheel" and fed_from(fed, machine, is_belt) then return true end
   end
   return false
end

---While the boiler is out of fuel, a goal that needs power says so instead.
---@return string?
local function out_of_fuel()
   for _, boiler in pairs(built("boiler")) do
      if fuelled(boiler) then return nil end
   end
   return "refuel"
end

---@param name string
---@return boolean
local function researched(name)
   return game.forces.player.technologies[name].researched
end

---The vanilla list's items in the cars' trunks and gun slots.
---@return table<string, integer>
local function packed()
   local counts = {}
   for _, car in pairs(built("car")) do
      for _, inventory in ipairs({ defines.inventory.car_trunk, defines.inventory.car_ammo }) do
         for _, item in pairs(assert(car.get_inventory(inventory)).get_contents()) do
            counts[item.name] = (counts[item.name] or 0) + item.count
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
   for _, wanted in ipairs(PACKING) do
      local lacking = wanted.count - (counts[wanted.name] or 0)
      if lacking > 0 then
         if #list > 1 then list[#list + 1] = ", " end
         list[#list + 1] = { "fa.campaign-07-item", lacking, prototypes.item[wanted.name].localised_name }
      end
   end
   return #list > 1 and list or nil
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

---@param fields LuaSurface.create_entity_param
---@return LuaEntity
local function create(fields)
   fields.force = "player"
   return assert(game.surfaces.nauvis.create_entity(fields))
end

---An empty stack for something given: in the inventory, or on the ground when it is full.
---@return LuaItemStack
local function free_stack()
   -- The character's own: in remote view the player has no inventory.
   local stack = assert(assert(player().character).get_main_inventory()).find_empty_stack()
   if stack then return stack end
   local surface = game.surfaces.nauvis
   local position = assert(surface.find_non_colliding_position("item-on-ground", player().position, 10, 0.5))
   return assert(surface.create_entity({ name = "item-on-ground", position = position, stack = "blueprint" })).stack
end

---@param data string
local function give_blueprint(data)
   local stack = free_stack()
   stack.set_stack("blueprint")
   assert(stack.import_stack(data) == 0)
end

---@param strings string[]
local function give_book(strings)
   local stack = free_stack()
   stack.set_stack("blueprint-book")
   local book = assert(stack.get_inventory(defines.inventory.item_main))
   for i, data in ipairs(strings) do
      book.insert("blueprint")
      assert(book[i].import_stack(data) == 0)
   end
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

   -- Facing north, the pump puts its water out on its south side.
   fixed({ name = "offshore-pump", position = PUMP, direction = defines.direction.north })
   for _, tile in ipairs(PIPE_TILES) do
      fixed({ name = "pipe", position = centre(tile, 1, 1) })
   end
   local boiler = fixed({ name = "boiler", position = centre(BOILER_TILE, 3, 2), direction = defines.direction.south })
   boiler.insert({ name = "coal", count = 50 })
   for _, tile in ipairs(ENGINE_TILES) do
      fixed({ name = "steam-engine", position = centre(tile, 3, 5), direction = defines.direction.north })
   end
   local drill = fixed({
      name = "electric-mining-drill",
      position = centre(COAL_DRILL_TILE, 3, 3),
      direction = defines.direction.west,
   })
   for _, tiles in ipairs({ IRON_DRILL_TILES, COPPER_DRILL_TILES }) do
      for _, tile in ipairs(tiles) do
         create({ name = "electric-mining-drill", position = centre(tile, 3, 3), direction = defines.direction.north })
         local furnace = create({ name = "stone-furnace", position = centre({ x = tile.x, y = tile.y - 2 }, 2, 2) })
         furnace.insert({ name = "coal", count = 20 })
      end
   end
   for _, tile in ipairs(POLE_TILES) do
      create({ name = "small-electric-pole", position = centre(tile, 1, 1) })
   end
   storage.coal_plant = { boiler = boiler, drill = drill } --[[@as fa.campaign.CoalPlant]]
   storage.watching_power = false --[[@as boolean]]
   storage.power_hinted = false --[[@as boolean]]

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
   force.chart(surface, { { -200, -200 }, { 200, 200 } })
end

---A hint that hands something out as it shows.
---@param name string
---@param give fun()
---@return fa.campaign.StoryNode
local function giving_node(name, give)
   local node = hint_node(name)
   local show = assert(node.action)
   node.action = function(event, story)
      give()
      show(event, story)
   end
   return node
end

-- Running short of power hands out the power block, once; the story hands it out at the latest
-- before the long research.
local function power_hint()
   if storage.power_hinted then return end
   storage.power_hinted = true
   give_blueprint(BLUEPRINTS.power)
   Helpers.hint({ "fa.campaign-07-low-power" })
end

---Whether an electric building has too little energy to work at full speed.
---@return boolean
local function short_of_power()
   for _, entity in
      pairs(game.surfaces.nauvis.find_entities_filtered({
         type = { "mining-drill", "inserter", "assembling-machine", "lab" },
         force = "player",
      }))
   do
      local buffer = entity.electric_buffer_size
      if buffer and entity.electric_network_id and entity.energy < buffer * 0.6 then return true end
   end
   return false
end

---@param goal string
---@param technology string
---@return fa.campaign.StoryNode
local function research_goal(goal, technology)
   return goal_node(goal, function()
      return researched(technology)
   end, { variant = out_of_fuel })
end

---@type fa.campaign.StoryNode[][]
local story_table = {
   {
      {
         init = setup,
         condition = story_elapsed_check(1),
      },
      hint_node("welcome"),
      hint_node("beacon"),
      hint_node("plant"),
      hint_node("drag"),
      hint_node("ruler"),
      goal_node("coal", boiler_on_belt, { variant = out_of_fuel }),
      hint_node("research"),
      goal_node("lab", function()
         for _, lab in pairs(built("lab")) do
            if lab.energy > 0 then return true end
         end
         return false
      end, {
         variant = out_of_fuel,
         after = function()
            storage.watching_power = true
         end,
      }),
      hint_node("labs-1"),
      hint_node("labs-2"),
      hint_node("technologies"),
      research_goal("wall", "stone-wall"),
      hint_node("car-1"),
      hint_node("car-2"),
      giving_node("assemblers", function()
         give_blueprint(BLUEPRINTS.science)
      end),
      hint_node("place"),
      hint_node("detach"),
      hint_node("ghosts"),
      goal_node("red", function()
         return fed_labs("automation-science-pack") >= 1
      end, { variant = out_of_fuel }),
      hint_node("copy"),
      goal_node("labs", function()
         return fed_labs("automation-science-pack") >= LABS_WANTED
      end, {
         params = function()
            return { fed_labs("automation-science-pack") }
         end,
         variant = out_of_fuel,
      }),
      giving_node("smelting", function()
         give_blueprint(BLUEPRINTS.smelter)
         give_blueprint(BLUEPRINTS.drills)
      end),
      hint_node("long-inserter"),
      goal_node("smelter", function()
         return belt_furnaces() >= FURNACES_WANTED
      end, {
         params = function()
            return { belt_furnaces() }
         end,
         variant = out_of_fuel,
      }),
      goal_node("plates", gears_from_belt, { variant = out_of_fuel }),
      research_goal("logistics", "logistics"),
      giving_node("belts", function()
         give_book(BLUEPRINTS.belts)
      end),
      hint_node("underground"),
      goal_node("underground", function()
         for _, belt in pairs(built_type("underground-belt")) do
            if belt.underground_belt_neighbour then return true end
         end
         return false
      end),
      hint_node("splitter"),
      goal_node("splitter", function()
         for _, splitter in pairs(built_type("splitter")) do
            local neighbours = splitter.belt_neighbours
            if #neighbours.inputs >= 1 and #neighbours.outputs >= 2 then return true end
         end
         return false
      end),
      research_goal("green-research", "logistic-science-pack"),
      hint_node("green"),
      goal_node("green", function()
         return fed_labs("logistic-science-pack") >= 1
      end, { variant = out_of_fuel }),
      {
         name = "power",
         condition = story_elapsed_check(1),
         action = power_hint,
      },
      hint_node("car-3"),
      research_goal("automobilism", "automobilism"),
      hint_node("engines"),
      goal_node("car", function()
         return find("car") ~= nil
      end),
      hint_node("driving"),
      goal_node("drive", function()
         return storage.distance >= DRIVE_WANTED
      end, {
         params = function()
            return { math.floor(storage.distance / 10) * 10 }
         end,
         start = function()
            storage.distance = 0 --[[@as number]]
         end,
      }),
      hint_node("enough"),
      goal_node("pack", function()
         return missing() == nil
      end, {
         params = function()
            return { missing() or "" }
         end,
         needs = "car",
      }),
      hint_node("go"),
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
   if player().character then story_update(storage.story, event) end
end

-- The drive counts the tiles the car covers with the player in it.
local function track_car()
   local distance = storage.distance --[[@as number?]]
   if not distance then return end
   local car = player().vehicle
   if not (car and car.type == "car") then
      storage.car_last = nil
      return
   end
   local position = car.position --[[@as MapPosition.struct]]
   local last = storage.car_last --[[@as MapPosition.struct?]]
   if last then
      local dx, dy = position.x - last.x, position.y - last.y
      storage.distance = distance + math.sqrt(dx * dx + dy * dy)
   end
   storage.car_last = position
end

script.on_event(defines.events.on_tick, function(event)
   if game.tick_paused then return end
   track_car()
   if storage.watching_power and not storage.power_hinted and event.tick % 269 == 0 and short_of_power() then
      power_hint()
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
   defines.events.on_built_entity,
   defines.events.on_research_finished,
}, update)
