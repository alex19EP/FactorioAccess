--Here: what the game's spidertron remote just did, said to the player who used it.
--
-- The remote is the game's own selection tool: select, add to or remove from the selection, send,
-- queue. The game changes the selection and the spidertrons before it raises these events, so they
-- only read the result. Speech only.

local Localising = require("scripts.localising")
local Speech = require("scripts.speech")

local mod = {}

---@param item string?
---@return boolean
local function is_remote(item)
   return item ~= nil and prototypes.item[item].type == "spidertron-remote"
end

---How many spidertrons the remote has selected, after a select, add or remove.
---@param event EventData.on_player_selected_area|EventData.on_player_alt_selected_area|EventData.on_player_reverse_selected_area
function mod.on_selected_area(event)
   if not is_remote(event.item) then return end
   local player = game.get_player(event.player_index)
   local selection = player.spidertron_remote_selection or {}
   Speech.speak(event.player_index, { "fa.spidertron-remote-selected", #selection })
end

---Where the selected spidertrons went: to follow an entity, to a point, or around a patrol.
---@param event EventData.on_player_used_spidertron_remote
function mod.on_used_remote(event)
   local player = game.get_player(event.player_index)
   local selection = player.spidertron_remote_selection or {}
   local spider = selection[1]
   if not spider then return end
   if spider.follow_target then
      Speech.speak(
         event.player_index,
         { "fa.spidertron-remote-following", Localising.get_localised_name_with_fallback(spider.follow_target) }
      )
   elseif spider.autopilot_patrol_size > 0 then
      Speech.speak(event.player_index, { "fa.spidertron-remote-patrol", spider.autopilot_patrol_size })
   elseif #spider.autopilot_destinations > 1 then
      Speech.speak(event.player_index, { "fa.spidertron-remote-waypoint", #spider.autopilot_destinations })
   else
      Speech.speak(event.player_index, { "fa.spidertron-remote-sent", #selection })
   end
end

return mod
