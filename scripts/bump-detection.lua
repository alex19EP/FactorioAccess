--[[
Bump sounds: the game itself decides each tick how a walking character's step went (Character::changePosition):
the whole way, slid along what it ran into, or not at all. The native DLL hands that to play_step_sounds for
this client's player:
- Trip: the first step that slides after walking freely (running into something)
- Slide: sliding on along it
- Stuck: blocked for 3 steps
Only this client knows those steps, so they play sounds and nothing else.
]]

local sounds = require("scripts.ui.sounds")

---@type fa.Native?
local native = rawget(_G, "fa_native")

local mod = {}

---@class fa.BumpDetection.StepState
---@field count integer? The step count last read
---@field last string The last step: "full", "partial" or "none"
---@field blocked integer Steps blocked in a row
---@field last_sound_tick integer

-- Per player, outside storage: it follows steps only this client knows.
---@type table<integer, fa.BumpDetection.StepState>
local step_states = {}

---Plays the trip, slide and stuck sounds from the game's own walking steps
---@param pindex integer
---@param this_tick integer
function mod.play_step_sounds(pindex, this_tick)
   if not native then return end
   local step, count = native.walking_step(pindex)
   local state = step_states[pindex]
   if not state then
      state = { last = "full", blocked = 0, last_sound_tick = 0 }
      step_states[pindex] = state
   end
   if not step then
      state.last = "full"
      state.blocked = 0
      return
   end
   if count == state.count then return end
   state.count = count

   if step == "none" then
      state.blocked = state.blocked + 1
      -- Stuck after 3 blocked steps, again every second while it lasts
      if state.blocked == 3 or (state.blocked > 3 and this_tick - state.last_sound_tick >= 60) then
         state.last_sound_tick = this_tick
         sounds.play_player_bump_stuck(pindex)
      end
   elseif step == "partial" then
      state.blocked = 0
      if state.last == "full" then
         state.last_sound_tick = this_tick
         sounds.play_player_bump_trip(pindex)
      elseif this_tick - state.last_sound_tick >= 30 then
         state.last_sound_tick = this_tick
         sounds.play_player_bump_slide(pindex)
      end
   else
      state.blocked = 0
   end
   state.last = step
end

return mod
