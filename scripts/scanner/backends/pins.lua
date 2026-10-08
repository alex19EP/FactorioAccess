--[[
Pins: the places, entities, tags, players and alerts a player pinned, which the game lists on the
right of the screen with a small camera each. They are read from the player when the scanner
refreshes, so there is nothing to track.
]]
local Alerts = require("scripts.alerts")
local ChartTags = require("scripts.scanner.backends.chart-tags")
local FaInfo = require("scripts.fa-info")
local FaUtils = require("scripts.fa-utils")
local Localising = require("scripts.localising")
local SC = require("scripts.scanner.scanner-consts")
local Speech = require("scripts.speech")

local mod = {}

---The surface a pin is on: its own, or that of what it points to. Nil for a pin that is on no
---surface.
---@param pin LuaPin
---@return integer?
local function pin_surface_index(pin)
   if pin.surface_index then return pin.surface_index end
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

---@param message fa.MessageBuilder
---@param player LuaPlayer
---@param pin LuaPin
local function describe_target(message, player, pin)
   local targets = pin.targets
   if targets[1] then
      if targets[1].type == "resource" then
         describe_resource_patch(message, targets)
      else
         message:list_item(FaInfo.ent_info(player.index, targets[1], true))
      end
      return
   end

   local tag = pin.chart_tag
   if tag then
      ChartTags.describe_tag(message, tag)
      return
   end

   local bound = pin.player
   if bound then
      message:list_item(bound.name)
      return
   end

   local alert_type = pin.alert_type
   if alert_type then
      local count = #pin.alert_positions
      message:list_item({ "gui-alert-tooltip." .. Alerts.get_alert_locale_key(alert_type), count })
   end
end

---@class fa.scanner.PinsBackend: fa.scanner.ScannerBackend
local PinsBackend = {}
mod.PinsBackend = PinsBackend
local PinsBackend_meta = { __index = PinsBackend }
if script then script.register_metatable("fa.scanner.PinsBackend", PinsBackend_meta) end

function PinsBackend.new()
   return setmetatable({}, PinsBackend_meta)
end

function PinsBackend:on_new_entity(e) end

function PinsBackend:on_entity_destroyed(event) end

function PinsBackend:on_new_tiles(tiles) end

---@param player LuaPlayer
---@param e fa.scanner.ScanEntry
function PinsBackend:validate_entry(player, e)
   local pin = e.backend_data
   return pin.valid and pin_surface_index(pin) == player.surface_index and pin.get_pin_center() ~= nil
end

---A pinned vehicle or player moves, and the pin with it.
---@param player LuaPlayer
---@param e fa.scanner.ScanEntry
function PinsBackend:update_entry(player, e)
   e.position = e.backend_data.get_pin_center()
end

---@param player LuaPlayer
---@param e fa.scanner.ScanEntry
function PinsBackend:readout_entry(player, e)
   local pin = e.backend_data
   local message = Speech.MessageBuilder.new()
   if pin.label ~= "" then message:list_item(pin.label) end
   describe_target(message, player, pin)
   message:list_item({ "fa.scanner-pin" })
   return message:build()
end

---@param player LuaPlayer
---@param callback fun(fa.scanner.ScanEntry)
function PinsBackend:dump_entries_to_callback(player, callback)
   for _, pin in ipairs(player.get_pins()) do
      local center = pin.get_pin_center()
      if center and pin_surface_index(pin) == player.surface_index then
         callback({
            position = center,
            backend = self,
            backend_data = pin,
            category = SC.CATEGORIES.PINS,
            subcategory = "pin",
         })
      end
   end
end

function PinsBackend:is_huge(e)
   return false
end

function PinsBackend:get_aabb(e)
   local p = e.position
   return p.x, p.y, p.x, p.y
end

return mod
