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
   local station = train.station
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
