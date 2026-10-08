--Here: Functions related to Kruise Kontrol
--
-- Jobs start from Kruise Kontrol's own control, CONTROL + ALT + mouse-button-2, which
-- helper-scripts/factorio-access-keys.ps1 also puts on CONTROL + ALT + RIGHTBRACKET. The game cursor
-- follows the FA cursor, so the job lands there. Enter cancels, also handled by Kruise Kontrol.
--
-- Kruise Kontrol says what it is doing with text it draws over the character: a remark that lasts
-- two seconds and is redrawn for every new one, and a status line, such as pathfinding, that keeps
-- its object while its text animates. Each new text object is spoken once.

local Speech = require("scripts.speech")
local StorageManager = require("scripts.storage-manager")

local MessageBuilder = Speech.MessageBuilder

local mod = {}

local interface_name = "kruise_kontrol"
local mod_name = "Kruise_Kontrol"

---@class fa.KruiseKontrol.State
---@field spoken table<uint64, true> Ids of the text objects already spoken

---@type table<integer, fa.KruiseKontrol.State>
local kk_storage = StorageManager.declare_storage_module("kruise_kontrol", {
   spoken = {},
})

function mod.is_active(pindex)
   if not remote.interfaces[interface_name] then return false end
   local character = game.get_player(pindex).character
   return character ~= nil and not remote.call(interface_name, "is_idle", character)
end

--FA actions to take when the enter/exit vehicle key is pressed
function mod.on_toggle_driving(pindex)
   local p = game.get_player(pindex)
   -- If in a car, make sure to stop it: the key both cancels Kruise Kontrol and exits the car.
   if p.vehicle and p.vehicle.type == "car" and p.vehicle.active == true then p.vehicle.speed = 0 end
end

---Speaks the Kruise Kontrol text that appeared over each player's character since the last call.
function mod.read_remarks()
   if not script.active_mods[mod_name] then return end

   local texts = {}
   for _, object in pairs(rendering.get_all_objects(mod_name)) do
      if object.type == "text" then table.insert(texts, object) end
   end

   for _, player in pairs(game.connected_players) do
      local character = player.character
      if character then
         local pindex = player.index
         local state = kk_storage[pindex]
         local spoken = {}
         local message = MessageBuilder.new()
         for _, object in ipairs(texts) do
            if object.target.entity == character then
               if not state.spoken[object.id] then message:list_item(object.text) end
               spoken[object.id] = true
            end
         end
         state.spoken = spoken
         local built = message:build()
         if built then Speech.speak(pindex, built) end
      end
   end
end

return mod
