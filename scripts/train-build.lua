--[[
What the game did with the rolling stock or train stop the player just built, which the preview in
hand does not show: the way a car heads on its rail, whether it joined the train beside it, and the
stop a train stands at; the way a stop faces once it turned to fit its rail. Other builds stay
silent, as the game's build sound says them.
]]
local BuildDimensions = require("scripts.build-dimensions")
local FaUtils = require("scripts.fa-utils")
local Speech = require("scripts.speech")

local mod = {}

---The stop the train's front stands at: one beside a rail under the train or the rail straight on
---past its front, near the front. The game sets LuaTrain.station only once an automatic train stops
---there.
---@param train LuaTrain
---@return LuaEntity?
local function stop_at_front(train)
   local front = train.front_end
   local rails = train.get_rails()
   local ahead = front.rail.get_connected_rail({
      rail_direction = front.direction,
      rail_connection_direction = defines.rail_connection_direction.straight,
   })
   if ahead then table.insert(rails, ahead) end
   local stops = front.rail.surface.find_entities_filtered({
      type = "train-stop",
      position = front.location.position,
      radius = 4,
   })
   for _, stop in ipairs(stops) do
      for _, rail in ipairs(rails) do
         if stop.connected_rail == rail then return stop end
      end
   end
   return nil
end

---@param entity LuaEntity
---@return LocalisedString
local function rolling_stock_words(entity)
   local message = Speech.MessageBuilder.new()
   message:list_item({ "fa.facing-direction", FaUtils.get_heading_info(entity) })
   local train = assert(entity.train)
   local cars = #train.carriages
   if cars > 1 then
      message:list_item({ "fa.train-build-connected", cars })
   else
      message:list_item({ "fa.train-build-not-connected" })
   end
   local station = train.station or stop_at_front(train)
   if station then message:list_item({ "fa.train-build-at-stop", assert(station.backer_name) }) end
   return message:build()
end

---@param event EventData.on_built_entity
---@param pindex integer
function mod.on_built_entity(event, pindex)
   local entity = event.entity
   if BuildDimensions.is_rolling_stock(entity.prototype) then
      Speech.speak(pindex, rolling_stock_words(entity))
   elseif entity.type == "train-stop" then
      local name = assert(entity.backer_name)
      Speech.speak(pindex, { "fa.train-build-stop", name, FaUtils.direction_lookup(entity.direction) })
   end
end

return mod
