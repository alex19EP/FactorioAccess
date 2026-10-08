--[[
What an electric pole is wired to and what its supply area powers, beside the game's window for it
(see entity-views.lua). The window shows the pole's whole network, but nothing of the pole itself:
these views show the pole's wires, then its supply area.
]]
local EntityViews = require("scripts.ui.entity-views")
local FaUtils = require("scripts.fa-utils")
local Localising = require("scripts.localising")

local mod = {}

-- An entity's name, after its quality but for normal: "rare small electric pole".
---@param name string
---@param quality string
---@return LocalisedString
local function name_with_quality(name, quality)
   local localised = Localising.get_localised_name_with_fallback(prototypes.entity[name])
   if quality == "normal" then return localised end
   return { "", Localising.get_localised_name_with_fallback(prototypes.quality[quality]), " ", localised }
end

-- The entities a pole's copper wires reach, nearest first.
---@param pole LuaEntity
---@return LuaEntity[]
function mod.wired_to(pole)
   local seen = {}
   local found = {}
   for _, connector in pairs(pole.get_wire_connectors(false)) do
      if connector.wire_type == defines.wire_type.copper then
         for _, connection in ipairs(connector.connections) do
            local other = connection.target.owner
            if not seen[other.unit_number] then
               seen[other.unit_number] = true
               table.insert(found, other)
            end
         end
      end
   end
   table.sort(found, function(a, b)
      return FaUtils.distance(pole.position, a.position) < FaUtils.distance(pole.position, b.position)
   end)
   return found
end

---@param pole LuaEntity
---@return LocalisedString[]
function mod.wire_cells(pole)
   local cells = { { "fa.pole-views-reach", pole.prototype.get_max_wire_distance(pole.quality) } }
   local wired = mod.wired_to(pole)
   for _, other in ipairs(wired) do
      table.insert(cells, {
         "fa.pole-views-wired",
         name_with_quality(other.name, other.quality.name),
         FaUtils.distance_speech_friendly(pole.position, other.position),
         FaUtils.direction_lookup(FaUtils.get_direction_biased(other.position, pole.position)),
      })
   end
   if not wired[1] then table.insert(cells, { "fa.pole-views-no-wires" }) end
   return cells
end

---@alias fa.PoleViews.Role "uses"|"produces"|"stores"

---@class fa.PoleViews.Supplied
---@field name string
---@field quality string
---@field role fa.PoleViews.Role
---@field count integer

-- What an electric building does with the network's energy.
---@param entity LuaEntity
---@return fa.PoleViews.Role
local function role_of(entity)
   if entity.type == "accumulator" then return "stores" end
   if entity.prototype.get_max_energy_production(entity.quality) > 0 then return "produces" end
   return "uses"
end

local ROLE_ORDER = { uses = 1, produces = 2, stores = 3 }

-- The electric buildings any part of which lies in the pole's supply area, grouped by name, quality
-- and what they do with the energy: those using it first, then producing and storing it, the most
-- numerous first within each.
---@param pole LuaEntity
---@return fa.PoleViews.Supplied[]
function mod.supplied(pole)
   local reach = pole.prototype.get_supply_area_distance(pole.quality)
   local p = pole.position
   local area = { { p.x - reach, p.y - reach }, { p.x + reach, p.y + reach } }
   ---@type table<string, fa.PoleViews.Supplied>
   local groups = {}
   for _, e in ipairs(pole.surface.find_entities_filtered({ area = area })) do
      if e.prototype.electric_energy_source_prototype then
         local role = role_of(e)
         local key = e.name .. ":" .. e.quality.name .. ":" .. role
         groups[key] = groups[key] or { name = e.name, quality = e.quality.name, role = role, count = 0 }
         groups[key].count = groups[key].count + 1
      end
   end
   local list = {}
   for _, g in pairs(groups) do
      table.insert(list, g)
   end
   table.sort(list, function(a, b)
      if a.role ~= b.role then return ROLE_ORDER[a.role] < ROLE_ORDER[b.role] end
      if a.count ~= b.count then return a.count > b.count end
      if a.name ~= b.name then return a.name < b.name end
      return a.quality < b.quality
   end)
   return list
end

---@param pole LuaEntity
---@return LocalisedString[]
function mod.supply_cells(pole)
   local size = pole.prototype.get_supply_area_distance(pole.quality) * 2
   local cells = { { "fa.pole-views-area", size } }
   local supplied = mod.supplied(pole)
   for _, s in ipairs(supplied) do
      table.insert(cells, {
         "fa.pole-views-building",
         name_with_quality(s.name, s.quality),
         s.count,
         { "fa.pole-views-" .. s.role },
      })
   end
   if not supplied[1] then table.insert(cells, { "fa.pole-views-nothing-supplied" }) end
   return cells
end

---@param pole LuaEntity
---@return fa.EntityViews.View[]
function mod.views(pole)
   return {
      { title = { "fa.pole-views-wires" }, columns = { { cells = mod.wire_cells(pole) } } },
      { title = { "fa.pole-views-supply" }, columns = { { cells = mod.supply_cells(pole) } } },
   }
end

EntityViews.register({ "electric-pole" }, defines.relative_gui_type.electric_network_gui, mod.views)

return mod
