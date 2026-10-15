local Speech = require("scripts.speech")
local StorageManager = require("scripts.storage-manager")

local mod = {}

---@class fa.WorkerRobots.State
---@field pending_logistic_state_announcement boolean
---@field pending_dispatch_state_announcement boolean

---@type table<number, fa.WorkerRobots.State>
local worker_robots_storage = StorageManager.declare_storage_module("worker_robots", {
   pending_logistic_state_announcement = false,
   pending_dispatch_state_announcement = false,
})

function mod.logistics_request_toggle_handler(pindex)
   -- The game's toggle-personal-logistic-requests control will handle the actual toggle
   -- We just set a flag to announce the new state on the next tick
   worker_robots_storage[pindex].pending_logistic_state_announcement = true
end

function mod.announce_robot_dispatch_status(pindex)
   -- The game's toggle-personal-roboport control will handle the actual toggle
   -- We just set a flag to announce the new state on the next tick
   worker_robots_storage[pindex].pending_dispatch_state_announcement = true
end

---On tick handler to announce logistics state changes after the game has processed them
---@param pindex integer
function mod.on_tick(pindex)
   local player_storage = worker_robots_storage[pindex]

   if player_storage.pending_logistic_state_announcement then
      player_storage.pending_logistic_state_announcement = false

      local p = game.get_player(pindex)
      local char = p.character
      if not char then return end

      local point = char.get_logistic_point(defines.logistic_member_index.character_requester)
      if not point then return end

      if point.enabled then
         Speech.speak(pindex, { "fa.robots-resumed-personal-logistics" })
      else
         Speech.speak(pindex, { "fa.robots-paused-personal-logistics" })
      end
   end

   if player_storage.pending_dispatch_state_announcement then
      player_storage.pending_dispatch_state_announcement = false

      local p = game.get_player(pindex)
      local char = p.character
      if not char then return end

      if char.allow_dispatching_robots then
         Speech.speak(pindex, { "fa.robots-dispatch-enabled" })
      else
         Speech.speak(pindex, { "fa.robots-dispatch-disabled" })
      end
   end
end

return mod
