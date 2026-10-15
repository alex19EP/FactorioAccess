--Here: Functions relating to circuit network signals.

local Localising = require("scripts.localising")
local ItemInfo = require("scripts.item-info")

local mod = {}

---Get the prototype dictionary for a signal type
---@param signal_type string
---@return table<string, any>
local function get_prototype_dict(signal_type)
   if signal_type == "item" or not signal_type then
      return prototypes.item
   elseif signal_type == "fluid" then
      return prototypes.fluid
   elseif signal_type == "virtual" then
      return prototypes.virtual_signal
   elseif signal_type == "entity" then
      return prototypes.entity
   elseif signal_type == "recipe" then
      return prototypes.recipe
   elseif signal_type == "space-location" then
      return prototypes.space_location
   elseif signal_type == "asteroid-chunk" then
      return prototypes.asteroid_chunk
   elseif signal_type == "quality" then
      return prototypes.quality
   else
      return {}
   end
end

---Localize a SignalID to a LocalisedString (without count)
---@param signal_id SignalID
---@return LocalisedString
function mod.localise_signal(signal_id)
   if not signal_id or not signal_id.name then return { "", "empty" } end

   local signal_type = signal_id.type or "item"
   local proto_dict = get_prototype_dict(signal_type)
   local proto = proto_dict[signal_id.name]

   if not proto then return { "", signal_id.name } end

   local base_name
   -- For items and fluids, use the quality-aware localisation
   if signal_type == "item" or signal_type == "fluid" then
      base_name = ItemInfo.item_or_fluid_info({
         name = proto,
         quality = signal_id.quality,
      }, proto_dict)
   else
      -- For other signals, just get the localised name
      base_name = Localising.get_localised_name_with_fallback(proto)
   end

   -- Add type suffix for entity and recipe signals to disambiguate
   if signal_type == "entity" then
      return { "", base_name, " ", { "fa.signal-suffix-entity" } }
   elseif signal_type == "recipe" then
      return { "", base_name, " ", { "fa.signal-suffix-recipe" } }
   else
      return base_name
   end
end

return mod
