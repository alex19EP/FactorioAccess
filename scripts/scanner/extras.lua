--[[
Scanner entries that only the Lua API reads, which the mod hands to the DLL's list itself: the
player's pins, the force's map tags, and the spots near the player where the offshore pump in hand
can be built.

They are read at refresh, on the client of the player who scans, and kept in a table of this module
outside storage: only that client has a scanner list. So this must only read the game, never change
it (see readout.lua). Each collection has a generation, which the DLL says back with each extra: an
automatic refresh collects while the list made from the one before is still in use.
]]
local CircuitNetwork = require("scripts.circuit-network")
local FaUtils = require("scripts.fa-utils")
local Localising = require("scripts.localising")
local SC = require("scripts.scanner.scanner-consts")
local Speech = require("scripts.speech")
local TileKinds = require("scripts.tile-kinds")

local mod = {}

---An entry of the mod's own: where it is, and how to check and say it later.
---@class fa.scanner.Extra
---@field category fa.scanner.Category
---@field key string its subcategory
---@field position MapPosition
---@field valid fun(): boolean
---@field readout fun(): fa.scanner.Readout
---@field said fa.scanner.Readout? its readout at refresh in words alone, for once it is gone

-- The gui-alert-tooltip locale key for each alert type, which takes the count
local ALERT_TOOLTIP_KEYS = {
   [defines.alert_type.entity_destroyed] = "destroyed",
   [defines.alert_type.entity_under_attack] = "attack",
   [defines.alert_type.turret_fire] = "turret-fire",
   [defines.alert_type.turret_out_of_ammo] = "turret-out-of-ammo",
   [defines.alert_type.train_no_path] = "train-no-path",
   [defines.alert_type.train_out_of_fuel] = "train-out-of-fuel",
   [defines.alert_type.no_material_for_construction] = "no-material-for-construction",
   [defines.alert_type.not_enough_construction_robots] = "not-enough-construction-robots",
   [defines.alert_type.not_enough_repair_packs] = "not-enough-repair-packs",
   [defines.alert_type.no_storage] = "no-storage",
   [defines.alert_type.no_platform_storage] = "no-platform-storage",
   [defines.alert_type.no_roboport_storage] = "no-roboport-storage",
   [defines.alert_type.collector_path_blocked] = "collector-path-blocked",
   [defines.alert_type.platform_tile_building_blocked] = "platform-tile-building-blocked",
   [defines.alert_type.pipeline_overextended] = "pipeline-overextended",
   [defines.alert_type.unclaimed_cargo] = "unclaimed-cargo",
   [defines.alert_type.custom] = "custom-alert",
}

---Speaks a tag as the map shows it: its text, then its icon.
---@param message fa.MessageBuilder
---@param tag LuaCustomChartTag
local function describe_tag(message, tag)
   if tag.text ~= "" then message:list_item(tag.text) end
   if tag.icon then message:list_item(CircuitNetwork.localise_signal(tag.icon)) end
end

---The surface a pin is on: its own, or that of what it points to. Nil for a pin that is on no
---surface.
---@param pin LuaPin
---@return integer?
function mod.pin_surface_index(pin)
   -- Typed uint32, but nil for a pin bound to an entity rather than a position
   ---@diagnostic disable-next-line: unnecessary-if
   if pin.surface_index then return pin.surface_index end
   ---@type LuaEntity?
   local target = pin.targets[1]
   if target then return target.surface_index end
   local tag = pin.chart_tag
   if tag then return tag.surface.index end
   local bound = pin.player
   if bound then return bound.surface_index end
   return nil
end

---What is left of a pinned resource patch, as the game's pin tooltip gives it.
---@param message fa.MessageBuilder
---@param resources LuaEntity[]
local function describe_resource_patch(message, resources)
   local prototype = resources[1].prototype
   local total = 0
   for _, resource in ipairs(resources) do
      total = total + resource.amount
   end
   local amount
   if prototype.infinite_resource then
      amount = { "fa.scanner-pin-resource-percent", math.floor(total / prototype.normal_resource_amount * 100) }
   else
      amount = FaUtils.format_number(total)
   end
   message:list_item({ "fa.scanner-pin-resource", Localising.get_localised_name_with_fallback(prototype), amount })
end

---A coordinate as the pins panel prints it, cut toward zero.
---@param value number
---@return integer
local function pin_coordinate(value)
   return value >= 0 and math.floor(value) or math.ceil(value)
end

---What an unlabelled pin is called, other than an entity the DLL reads, as the pins panel names it
---(Pin::str): its alerts, its tag, its place, its player, or its resource patch.
---@param message fa.MessageBuilder
---@param pin LuaPin
local function describe_target(message, pin)
   local alert_type = pin.alert_type
   if alert_type then
      local count = #pin.alert_positions
      message:list_item({ "gui-alert-tooltip." .. ALERT_TOOLTIP_KEYS[alert_type], count })
      return
   end

   local tag = pin.chart_tag
   if tag then
      describe_tag(message, tag)
      return
   end

   local position = pin.position
   if position then
      local point = position --[[@as MapPosition.struct]]
      message:list_item(string.format("{%d, %d}", pin_coordinate(point.x), pin_coordinate(point.y)))
      return
   end

   local bound = pin.player
   if bound then
      message:list_item(bound.name)
      return
   end

   local targets = pin.targets
   if #targets > 0 then describe_resource_patch(message, targets) end
end

---The places, entities, tags, players and alerts the player pinned, which the game lists on the
---right of the screen with a small camera each.
---@param player LuaPlayer
---@return fa.scanner.Extra[]
function mod.pins(player)
   local extras = {}
   for _, pin in ipairs(player.get_pins()) do
      local center = pin.get_pin_center()
      if center and mod.pin_surface_index(pin) == player.surface_index then
         table.insert(extras, {
            category = SC.CATEGORIES.PINS,
            key = "pin",
            position = center,
            valid = function()
               return pin.valid and mod.pin_surface_index(pin) == player.surface_index and pin.get_pin_center() ~= nil
            end,
            readout = function()
               local label = pin.label ~= "" and pin.label or nil
               -- The game points an arrow at an always visible pin from the edge of the screen
               local sound = pin.always_visible and "scanner-pin-arrow" or nil
               local target = pin.targets[1]
               if target and target.type ~= "resource" then
                  return { words = label, entity = target, after = { "fa.scanner-pin" }, sound = sound }
               end
               local message = Speech.MessageBuilder.new()
               -- The game names a labelled pin by its label alone
               if label then
                  message:list_item(label)
               else
                  describe_target(message, pin)
               end
               message:list_item({ "fa.scanner-pin" })
               return { words = message:build(), sound = sound }
            end,
         })
      end
   end
   return extras
end

---The labelled markers the player's force put on the map, which the map shows and its search finds.
---@param player LuaPlayer
---@return fa.scanner.Extra[]
function mod.tags(player)
   local extras = {}
   for _, tag in pairs(player.force.find_chart_tags(player.surface)) do
      table.insert(extras, {
         category = SC.CATEGORIES.TAGS,
         key = "tag",
         position = tag.position,
         valid = function()
            return tag.valid and tag.surface.index == player.surface_index
         end,
         readout = function()
            local message = Speech.MessageBuilder.new()
            describe_tag(message, tag)
            message:list_item({ "fa.scanner-tag" })
            return { words = message:build() }
         end,
      })
   end
   return extras
end

-- An offshore pump stands on the land tile next to the water and faces it. Each facing is the step
-- from that land tile to the water tile.
local PUMP_FACINGS = {
   { x = 0, y = -1, direction = defines.direction.north },
   { x = 1, y = 0, direction = defines.direction.east },
   { x = 0, y = 1, direction = defines.direction.south },
   { x = -1, y = 0, direction = defines.direction.west },
}

---@param player LuaPlayer
---@param pump LuaEntityPrototype
---@param position MapPosition
---@param direction defines.direction
local function can_place_pump(player, pump, position, direction)
   return player.surface.can_place_entity({
      name = pump.name,
      position = position,
      direction = direction,
      force = player.force,
      build_check_type = defines.build_check_type.manual,
   })
end

---The offshore pump the player holds, if any.
---@param player LuaPlayer
---@return LuaEntityPrototype?
local function pump_in_hand(player)
   local stack = player.cursor_stack
   if not stack or not stack.valid_for_read then return nil end
   local place = stack.prototype.place_result
   if place and place.type == "offshore-pump" then return place end
   return nil
end

---Every land tile near the player where the offshore pump in hand can be built facing the water
---next to it. The game turns the pump to face the water itself, so the direction is only read out.
---@param player LuaPlayer
---@return fa.scanner.Extra[]
function mod.pump_spots(player)
   local extras = {}
   local pump = pump_in_hand(player)
   if not pump then return extras end

   local water = {}
   local tiles = player.surface.find_tiles_filtered({
      position = player.position,
      radius = SC.PUMP_SPOT_DISTANCE,
      name = TileKinds.pumpable(),
   })
   for _, tile in ipairs(tiles) do
      water[tile.position.x .. "," .. tile.position.y] = true
   end

   local found = {}
   for _, tile in ipairs(tiles) do
      local x, y = tile.position.x, tile.position.y
      for _, facing in ipairs(PUMP_FACINGS) do
         local land_x, land_y = x - facing.x, y - facing.y
         local key = land_x .. "," .. land_y
         local position = { x = land_x + 0.5, y = land_y + 0.5 }
         if not found[key] and not water[key] and can_place_pump(player, pump, position, facing.direction) then
            found[key] = true
            table.insert(extras, {
               category = SC.CATEGORIES.BUILD_SPOTS,
               key = "offshore-pump-spot",
               position = position,
               valid = function()
                  return can_place_pump(player, pump, position, facing.direction)
               end,
               readout = function()
                  return {
                     words = {
                        "fa.scanner-pump-spot",
                        Localising.get_localised_name_with_fallback(pump),
                        FaUtils.direction_lookup(facing.direction),
                     },
                  }
               end,
            })
         end
      end
   end
   return extras
end

---A readout in words alone, which outlives its entity: the entity by its name.
---@param readout fa.scanner.Readout
---@return fa.scanner.Readout
local function in_words(readout)
   if not readout.entity then return readout end
   local message = Speech.MessageBuilder.new()
   message:list_item(readout.words)
   message:list_item(Localising.get_localised_name_with_fallback(readout.entity))
   message:list_item(readout.after)
   return { words = message:build() }
end

-- The extras of each player's latest collections by generation, on that player's client: the one
-- the list in use was made from, and the one an automatic refresh is making a list from, which may
-- be two on when a refresh was dropped for another.
---@type table<integer, table<integer, fa.scanner.Extra[]>>
local kept = {}

---Everything the mod lists itself for `player`, kept for readouts as collection `generation`, and
---as the DLL takes it.
---@param player LuaPlayer
---@param generation integer
---@return { category: string, key: string, x: number, y: number }[]
function mod.collect(player, generation)
   local extras = {}
   for _, list in ipairs({ mod.pins(player), mod.tags(player), mod.pump_spots(player) }) do
      for _, extra in ipairs(list) do
         table.insert(extras, extra)
      end
   end
   local generations = kept[player.index] or {}
   kept[player.index] = {
      [generation - 2] = generations[generation - 2],
      [generation - 1] = generations[generation - 1],
      [generation] = extras,
   }
   local listed = {}
   for i, extra in ipairs(extras) do
      extra.said = in_words(extra.readout())
      listed[i] = { category = extra.category, key = extra.key, x = extra.position.x, y = extra.position.y }
   end
   return listed
end

---What to say of extra `id` (its one-based place in collection `generation`): fresh while it is
---there, else what it was.
---@param pindex integer
---@param generation integer
---@param id integer
---@return fa.scanner.Readout?
function mod.readout(pindex, generation, id)
   local extra = ((kept[pindex] or {})[generation] or {})[id]
   if not extra then return nil end
   if not extra.valid() then return extra.said end
   local readout = extra.readout()
   extra.said = in_words(readout)
   return readout
end

return mod
