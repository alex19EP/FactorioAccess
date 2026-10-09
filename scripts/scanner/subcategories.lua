--[[
Scanner subcategories that say more than the prototype: what a machine makes, what a chest or
pipe holds, which train a wagon is in. The DLL lists the entities and leaves these types to the
Lua API, which reads all of it (native/src/scanner.cpp kDetailedTypes).

This runs only on the client of the player who scans, so it must only read: no storage, no change
to any entity or player, no math.random (its state is shared). Any of those would desync that client.
]]
local BuildingTools = require("scripts.building-tools")
local Info = require("scripts.fa-info")
local ResourceMining = require("scripts.resource-mining")

local mod = {}

-- For things such as crafting machines, trains, etc. the subcategory is
-- 'prototype/recipe', 'prototype/train-name', etc.
local function cat2(c1, c2)
   return string.format("%s/%s", c1, c2)
end

---@param ent LuaEntity
local function crafting_machine(ent)
   local r = ent.get_recipe()
   local rn = r and r.name or "<UNCONFIGURED>"
   return cat2(ent.name, rn)
end

---@param ent LuaEntity
local function mining_drill(ent)
   local under_drill = ResourceMining.compute_resources_under_drill(ent)
   local keys = {}
   for k in pairs(under_drill) do
      table.insert(keys, k)
   end
   table.sort(keys)
   return cat2(ent.name, table.concat(keys, "/"))
end

-- A furnace that has no recipe yet may still hold what it made.
---@param ent LuaEntity
local function furnace(ent)
   local recipe = ent.get_recipe()
   local rname = recipe and recipe.name or nil
   local oi = ent.get_output_inventory()
   if not rname and #oi > 0 and oi[1].valid_for_read then rname = oi[1].name end
   return cat2(ent.name, rname or "<UNCONFIGURED>")
end

-- Rolling stock is grouped by train, so that the scanner isn't cluttered with every car.
---@param ent LuaEntity
local function rolling_stock(ent)
   return cat2("train", tostring(ent.train.id))
end

---@param ent LuaEntity
local function ghost(ent)
   return ent.ghost_type
end

-- Spawners are grouped by how polluted they are.
local SPAWNER_POLLUTION_BUCKETS = {
   { 0, { "fa.scanner-spawner-polluted-none" } },
   { 1, { "fa.scanner-spawner-polluted-lightly" } },
   { 99, { "fa.scanner-spawner-polluted-heavily" } },
}

---@param ent LuaEntity
---@return integer
local function pollution_level(ent)
   local level = 1
   local p = ent.absorbed_pollution
   for i = 1, #SPAWNER_POLLUTION_BUCKETS do
      if SPAWNER_POLLUTION_BUCKETS[i][1] <= p then
         level = i
      else
         break
      end
   end
   return level
end

---@param ent LuaEntity
local function spawner(ent)
   return cat2(ent.name, tostring(pollution_level(ent)))
end

-- Containers group by what they hold: nothing, one item (of any qualities), or a mix.
---@param ent LuaEntity
local function container(ent)
   local subcat = "<EMPTY>"
   for _, stack in ipairs(ent.get_inventory(defines.inventory.chest).get_contents()) do
      if subcat == "<EMPTY>" then
         subcat = stack.name
      elseif subcat ~= stack.name then
         subcat = "<MIXED>"
         break
      end
   end
   return cat2(ent.name, subcat)
end

-- In the rare case of several fluids, one of them, not always the same.
---@param ent LuaEntity
local function with_fluid(ent)
   local fluid_name = next(ent.get_fluid_contents()) or "<NONE>"
   return cat2(ent.name, fluid_name)
end

---@param ent LuaEntity
local function roboport(ent)
   return cat2(ent.name, ent.backer_name)
end

---@param ent LuaEntity
local function pipe(ent)
   local fluid = next(ent.get_fluid_contents()) or "<NONE>"
   local end_part = BuildingTools.is_a_pipe_end(ent) and "<END>" or "<NONE>"
   return string.format("%s/%s/%s", ent.name, fluid, end_part)
end

---@type table<string, fun(ent: LuaEntity): string>
local BY_TYPE = {
   ["artillery-wagon"] = rolling_stock,
   ["assembling-machine"] = crafting_machine,
   ["cargo-wagon"] = rolling_stock,
   ["container"] = container,
   ["entity-ghost"] = ghost,
   ["fluid-wagon"] = rolling_stock,
   ["furnace"] = furnace,
   ["infinity-container"] = container,
   ["infinity-pipe"] = with_fluid,
   ["locomotive"] = rolling_stock,
   ["logistic-container"] = container,
   ["mining-drill"] = mining_drill,
   ["pipe"] = pipe,
   ["pipe-to-ground"] = with_fluid,
   ["roboport"] = roboport,
   ["storage-tank"] = with_fluid,
   ["tile-ghost"] = ghost,
   ["unit-spawner"] = spawner,
}

-- The subcategory of each entity the DLL handed over ({name, x, y} each), in its order: a string,
-- or false where the entity is gone.
---@param surface LuaSurface
---@param details { name: string, x: number, y: number }[]
---@return (string|false)[]
function mod.keys(surface, details)
   local keys = {}
   for i, detail in ipairs(details) do
      local ent = surface.find_entities_filtered({
         position = { x = detail.x, y = detail.y },
         radius = 1 / 512,
         name = detail.name,
         limit = 1,
      })[1]
      local by_type = ent and BY_TYPE[ent.type]
      keys[i] = by_type and by_type(ent) or false
   end
   return keys
end

-- What the scanner says of an entity: fa-info's scanner readout, with a spawner's pollution.
---@param pindex integer
---@param ent LuaEntity
---@return LocalisedString
function mod.readout(pindex, ent)
   local info = Info.ent_info(pindex, ent, true)
   if ent.type == "unit-spawner" then
      return { "fa.scanner-spawner-announce", info, SPAWNER_POLLUTION_BUCKETS[pollution_level(ent)][2] }
   end
   return info
end

return mod
