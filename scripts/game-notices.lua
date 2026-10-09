--Here: Game events that vanilla shows on screen and the Lua API reports, spoken as they happen.
--
-- The goal window (top left, set by tutorials, the campaign and scenarios) has no event, so it is
-- polled. Achievements and map tags other players add come as events. A space platform arriving or
-- leaving is not here: vanilla shows no message for it, only the platform's state in the Space
-- Platforms window and the space map.

local Speech = require("scripts.speech")
local StorageManager = require("scripts.storage-manager")

local mod = {}

---@class fa.GameNotices.State
---@field goal string? The goal description last spoken, serialized; nil before the first check

---@type table<integer, fa.GameNotices.State>
local notices_storage = StorageManager.declare_storage_module("game_notices", {})

---What to say about the player's goal window if it changed since the last check, else nil. An
---emptied window is remembered but not spoken: it just disappears.
---@param pindex integer
---@return LocalisedString?
function mod.check_goal(pindex)
   local goal = game.get_player(pindex).get_goal_description()
   local serialized = serpent.line(goal)
   local state = notices_storage[pindex]
   if state.goal == serialized then return nil end
   state.goal = serialized
   if goal == "" then return nil end
   return { "fa.notice-goal", goal }
end

function mod.on_tick()
   for _, player in pairs(game.connected_players) do
      local goal = mod.check_goal(player.index)
      if goal then Speech.speak(player.index, goal) end
   end
end

---@param event EventData.on_achievement_gained
---@param pindex integer
function mod.on_achievement_gained(event, pindex)
   Speech.speak(pindex, { "fa.notice-achievement", event.achievement.localised_name })
end

---A tag another player of the force put on the map. Tags the player adds themselves they know of.
---@param event EventData.on_chart_tag_added
function mod.on_chart_tag_added(event)
   if not event.player_index then return end
   local author = game.get_player(event.player_index)
   local text = event.tag.text
   for _, player in pairs(event.force.connected_players) do
      if player.index ~= event.player_index then
         if text == "" then
            Speech.speak(player.index, { "fa.notice-map-tag", author.name })
         else
            Speech.speak(player.index, { "fa.notice-map-tag-text", author.name, text })
         end
      end
   end
end

return mod
