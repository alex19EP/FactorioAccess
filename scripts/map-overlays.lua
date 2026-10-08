--[[
Map overlays: what the map draws over a cell, said after what the cell holds.

The game's map view options (a panel at the right of remote view, a part of Ctrl+Tab) toggle
overlays on the map: coverage of electric and logistic networks, turret ranges, pollution, station,
player and tag names, rail signal states, the recipes machines make and the fluids in pipes. They
are this client's config, read through fa_native.map_overlays, so they change only what is said.

Worker robots have no words of their own: the cell's summary already counts them.
]]
local Localising = require("scripts.localising")

local mod = {}

local TURRET_TYPES = { "ammo-turret", "electric-turret", "fluid-turret", "artillery-turret" }
local PIPELINE_TYPES = { "pipe", "pipe-to-ground", "pump", "storage-tank" }

---The farthest any prototype of `types` reaches, by `reach(prototype)`.
---@param types string[]
---@param reach fun(proto: LuaEntityPrototype): number?
---@return number
local function farthest(types, reach)
   local most = 0
   for _, proto in pairs(prototypes.get_entity_filtered({ { filter = "type", type = types } })) do
      most = math.max(most, reach(proto) or 0)
   end
   return most
end

---@param left_top MapPosition
---@param right_bottom MapPosition
---@param margin number
---@return BoundingBox
local function grown(left_top, right_bottom, margin)
   return {
      { left_top.x - margin, left_top.y - margin },
      { right_bottom.x + margin, right_bottom.y + margin },
   }
end

---Whether the square of half size `radius` around `position` overlaps the cell.
local function square_overlaps(position, radius, left_top, right_bottom)
   return position.x + radius > left_top.x
      and position.x - radius < right_bottom.x
      and position.y + radius > left_top.y
      and position.y - radius < right_bottom.y
end

---Whether the circle of `radius` around `position` overlaps the cell.
local function circle_overlaps(position, radius, left_top, right_bottom)
   local dx = math.max(left_top.x - position.x, 0, position.x - right_bottom.x)
   local dy = math.max(left_top.y - position.y, 0, position.y - right_bottom.y)
   return dx * dx + dy * dy <= radius * radius
end

---@param set table<any, true>
local function count(set)
   local n = 0
   for _ in pairs(set) do
      n = n + 1
   end
   return n
end

local function electric_networks(surface, force, left_top, right_bottom)
   local reach = farthest({ "electric-pole" }, function(proto)
      return proto.get_supply_area_distance()
   end)
   local networks = {}
   for _, pole in
      pairs(surface.find_entities_filtered({
         type = "electric-pole",
         force = force,
         area = grown(left_top, right_bottom, reach),
      }))
   do
      local radius = pole.prototype.get_supply_area_distance(pole.quality)
      if radius > 0 and square_overlaps(pole.position, radius, left_top, right_bottom) then
         networks[pole.electric_network_id] = true
      end
   end
   return count(networks)
end

local function logistic_networks(surface, force, left_top, right_bottom)
   local reach = farthest({ "roboport" }, function(proto)
      return proto.logistic_radius
   end)
   local networks = {}
   for _, roboport in
      pairs(surface.find_entities_filtered({
         type = "roboport",
         force = force,
         area = grown(left_top, right_bottom, reach),
      }))
   do
      local cell = roboport.logistic_cell
      local network = cell.logistic_network
      if network and square_overlaps(roboport.position, cell.logistic_radius, left_top, right_bottom) then
         networks[network.network_id] = true
      end
   end
   return count(networks)
end

local function turrets_in_range(surface, force, left_top, right_bottom)
   local reach = farthest(TURRET_TYPES, function(proto)
      return proto.turret_range
   end)
   local n = 0
   for _, turret in
      pairs(surface.find_entities_filtered({
         type = TURRET_TYPES,
         force = force,
         area = grown(left_top, right_bottom, reach),
      }))
   do
      if circle_overlaps(turret.position, turret.prototype.turret_range, left_top, right_bottom) then n = n + 1 end
   end
   return n
end

local function pollution(surface, left_top, right_bottom)
   local total = 0
   for cx = math.floor(left_top.x / 32), math.ceil(right_bottom.x / 32) - 1 do
      for cy = math.floor(left_top.y / 32), math.ceil(right_bottom.y / 32) - 1 do
         total = total + surface.get_pollution({ cx * 32 + 16, cy * 32 + 16 })
      end
   end
   return math.floor(total + 0.5)
end

---Names and how often each comes up, most first.
---@param counts table<string, { name: LocalisedString, count: integer }>
---@return { name: LocalisedString, count: integer }[]
local function by_count(counts)
   local list = {}
   for _, entry in pairs(counts) do
      table.insert(list, entry)
   end
   table.sort(list, function(a, b)
      return a.count > b.count
   end)
   return list
end

---@param counts table<string, { name: LocalisedString, count: integer }>
local function tally(counts, key, name)
   local entry = counts[key]
   if entry then
      entry.count = entry.count + 1
   else
      counts[key] = { name = name, count = 1 }
   end
end

local function recipes(surface, force, area)
   local counts = {}
   for _, machine in
      pairs(surface.find_entities_filtered({
         type = { "assembling-machine", "furnace", "rocket-silo" },
         force = force,
         area = area,
      }))
   do
      local recipe = machine.get_recipe()
      if recipe then tally(counts, recipe.name, Localising.get_localised_name_with_fallback(recipe)) end
   end
   return by_count(counts)
end

local function fluids(surface, force, area)
   local counts = {}
   for _, entity in pairs(surface.find_entities_filtered({ type = PIPELINE_TYPES, force = force, area = area })) do
      for name in pairs(entity.get_fluid_contents()) do
         tally(counts, name, Localising.get_localised_name_with_fallback(prototypes.fluid[name]))
      end
   end
   return by_count(counts)
end

local SIGNAL_STATES = {
   { state = defines.signal_state.closed, key = "fa.map-overlay-signals-closed" },
   { state = defines.signal_state.reserved, key = "fa.map-overlay-signals-reserved" },
   { state = defines.signal_state.reserved_by_circuit_network, key = "fa.map-overlay-signals-closed-by-circuit" },
   { state = defines.signal_state.open, key = "fa.map-overlay-signals-open" },
}

---Appends what the overlays that are on show over the cell from `left_top` to `right_bottom`
---(exclusive) to `message`, each a list item.
---@param message fa.MessageBuilder
---@param player LuaPlayer
---@param left_top MapPosition
---@param right_bottom MapPosition
---@param overlays table<string, true> The overlays that are on, as fa_native.map_overlays gives them
function mod.describe(message, player, left_top, right_bottom, overlays)
   local surface = player.surface
   local force = player.force
   local area = { left_top, right_bottom }

   if overlays.pollution and surface.pollutant_type then
      local amount = pollution(surface, left_top, right_bottom)
      if amount > 0 then
         message:list_item({
            "fa.map-overlay-pollution",
            amount,
            Localising.get_localised_name_with_fallback(surface.pollutant_type),
         })
      end
   end

   if overlays.electric_network then
      local n = electric_networks(surface, force, left_top, right_bottom)
      if n > 0 then message:list_item({ "fa.map-overlay-electric-networks", n }) end
   end

   if overlays.logistic_network then
      local n = logistic_networks(surface, force, left_top, right_bottom)
      if n > 0 then message:list_item({ "fa.map-overlay-logistic-networks", n }) end
   end

   if overlays.turret_range then
      local n = turrets_in_range(surface, force, left_top, right_bottom)
      if n > 0 then message:list_item({ "fa.map-overlay-turrets", n }) end
   end

   if overlays.station_names then
      for _, stop in pairs(surface.find_entities_filtered({ type = "train-stop", area = area })) do
         message:list_item({ "fa.map-overlay-station", stop.backer_name })
      end
   end

   if overlays.player_names then
      for _, other in pairs(game.connected_players) do
         if other.index ~= player.index and other.surface == surface then
            local position = other.physical_position
            if
               position.x >= left_top.x
               and position.x < right_bottom.x
               and position.y >= left_top.y
               and position.y < right_bottom.y
            then
               message:list_item({ "fa.map-overlay-player", other.name })
            end
         end
      end
   end

   if overlays.tags then
      for _, tag in pairs(force.find_chart_tags(surface, area)) do
         local name = tag.text
         if name == "" and tag.icon then name = tag.icon.name end
         message:list_item({ "fa.map-overlay-tag", name })
      end
   end

   if overlays.rail_signal_states then
      local signals = surface.find_entities_filtered({
         type = { "rail-signal", "rail-chain-signal" },
         force = force,
         area = area,
      })
      local states = {}
      for _, signal in pairs(signals) do
         states[signal.signal_state] = (states[signal.signal_state] or 0) + 1
      end
      for _, entry in ipairs(SIGNAL_STATES) do
         local n = states[entry.state]
         if n then message:list_item({ entry.key, n }) end
      end
   end

   if overlays.recipe_icons then
      local made = recipes(surface, force, area)
      if next(made) then
         message:list_item({ "fa.map-overlay-making" })
         for i, entry in ipairs(made) do
            local item = { "fa.map-overlay-count", entry.count, entry.name }
            if i == 1 then
               message:fragment(item)
            else
               message:list_item(item)
            end
         end
      end
   end

   if overlays.pipelines then
      local carried = fluids(surface, force, area)
      if next(carried) then
         message:list_item({ "fa.map-overlay-pipes" })
         for i, entry in ipairs(carried) do
            if i == 1 then
               message:fragment(entry.name)
            else
               message:list_item(entry.name)
            end
         end
      end
   end
end

return mod
