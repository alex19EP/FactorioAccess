--[[
Bump Detection System

Sounds: the game itself decides each tick how a walking character's step went (Character::changePosition):
the whole way, slid along what it ran into, or not at all. The native DLL hands that to play_step_sounds for
this client's player:
- Trip: the first step that slides after walking freely (running into something)
- Slide: sliding on along it
- Stuck: blocked for 3 steps
Only this client knows those steps, so they play sounds and nothing else.

Bumps for the mod's own logic (build lock) come from the movement history in the game state, the same on every
client:
1. Deviation Angle: Compares intended direction (from player input) with actual movement direction.
   - High deviation (>20°) while moving smoothly = sliding along a wall
2. Path Angle: Compares consecutive movement vectors to detect sharp turns.
   - High path angle (>30°) = hitting a corner or obstacle
3. Stuck Detection: Detects when walking input is active but position is unchanged.
   - Same position for 3+ ticks while walking = stuck against wall
]]

local StorageManager = require("scripts.storage-manager")
local MovementHistory = require("scripts.movement-history")
local sounds = require("scripts.ui.sounds")

---@type fa.Native?
local native = rawget(_G, "fa_native")
local Logging = require("scripts.logging")
local Consts = require("scripts.consts")
local Geometry = require("scripts.geometry")

local logger = Logging.Logger("bump-detection")
local mod = {}

-- Callbacks for bump events
---@type function[]
local bump_callbacks = {}

---@class fa.BumpDetection.BumpState
---@field last_bump_tick number
---@field last_stuck_tick number

---@type table<number, fa.BumpDetection.BumpState>
local bump_storage = StorageManager.declare_storage_module("bump_detection", {
   last_bump_tick = 0,
   last_stuck_tick = 0,
})

---Resets bump detection statistics for a player
---@param pindex number
function mod.reset_bump_stats(pindex)
   bump_storage[pindex] = {
      last_bump_tick = 0,
      last_stuck_tick = 0,
   }
end

---Register a callback to be called when a bump is detected
---@param callback function
function mod.register_bump_callback(callback)
   table.insert(bump_callbacks, callback)
end

---Notify all registered callbacks of a bump
---@param pindex number
local function notify_bump_callbacks(pindex)
   for _, callback in ipairs(bump_callbacks) do
      callback(pindex)
   end
end

---Tells the bump callbacks when the movement history shows a collision
---@param pindex number
---@param this_tick number
function mod.check_bump(pindex, this_tick)
   local bump = bump_storage[pindex]

   -- Check cooldown (30 ticks = 0.5 seconds) to avoid sound spam
   if this_tick - bump.last_bump_tick < 30 then return end

   -- Get movement history
   local reader = MovementHistory.get_movement_history_reader(pindex)

   -- Collect recent walking entries from movement history
   -- We look at the last 5 ticks to get enough data for analysis
   local entries = {} -- Full movement entries
   local positions = {} -- Just the positions for easier access
   local intended_direction = nil -- The direction the player is trying to move

   for i = 0, 4 do
      local entry = reader:get(i)
      if entry then
         logger:debug(
            string.format(
               "Entry %d: kind=%s, pos=(%.2f,%.2f), dir=%s, vel=%.3f",
               i,
               entry.kind or "nil",
               entry.position and entry.position.x or 0,
               entry.position and entry.position.y or 0,
               entry.direction or "nil",
               entry.velocity or 0
            )
         )
      else
         logger:debug(string.format("Entry %d: nil", i))
      end

      if entry and entry.kind == MovementHistory.MOVEMENT_KINDS.WALKING then
         table.insert(entries, entry)
         table.insert(positions, entry.position)
         intended_direction = intended_direction or entry.direction
         assert(intended_direction)
      end
   end

   -- Need at least 3 walking entries to detect bump
   -- This ensures we have enough data to calculate movement vectors
   if #entries < 3 then
      logger:debug(string.format("Not enough entries: %d < 3", #entries))
      return
   end

   -- Check if all entries have same intended direction
   -- We only detect bumps when the player is consistently trying to move in one direction
   -- This avoids false positives when the player is intentionally changing direction
   local consistent_direction = true
   for _, entry in ipairs(entries) do
      if entry.direction ~= intended_direction then
         consistent_direction = false
         break
      end
   end

   -- Only detect bumps when trying to maintain direction
   if not consistent_direction then
      logger:debug("Inconsistent direction, skipping")
      return
   end

   -- Calculate actual movement vector and normalize it to get direction
   -- This tells us which direction the player actually moved (not what they intended)
   local dx = positions[1].x - positions[2].x -- Most recent movement
   local dy = positions[1].y - positions[2].y
   local actual_x, actual_y = Geometry.normalize_2d(dx, dy)

   if actual_x == 0 and actual_y == 0 then
      -- No movement, possibly stuck (handled by stuck detection)
      return
   end

   -- Get intended movement direction as a unit vector
   -- This is where the player WANTS to go based on their input
   ---@cast intended_direction defines.direction
   local intended_x, intended_y = Geometry.uv_for_direction(intended_direction)

   logger:debug(string.format("Intended: (%.2f,%.2f), Actual: (%.2f,%.2f)", intended_x, intended_y, actual_x, actual_y))

   -- Calculate deviation angle: how far off is actual movement from intended?
   -- 0° = moving exactly where intended, 90° = perpendicular (sliding along wall)
   local angle = Geometry.angle_between_2d(intended_x, intended_y, actual_x, actual_y)
   local angle_degrees = Geometry.radians_to_degrees(angle)

   logger:debug(string.format("Deviation angle: %.1f degrees", angle_degrees))

   -- Check for path deviation (sharp turns in actual movement)
   -- This detects when the movement path itself changes direction sharply
   local v1x = positions[1].x - positions[2].x -- Movement from tick -1 to tick 0
   local v1y = positions[1].y - positions[2].y
   local v2x = positions[2].x - positions[3].x -- Movement from tick -2 to tick -1
   local v2y = positions[2].y - positions[3].y

   local path_angle = Geometry.angle_between_2d(v1x, v1y, v2x, v2y)
   local path_angle_degrees = Geometry.radians_to_degrees(path_angle)

   logger:debug(string.format("Path angle: %.1f degrees", path_angle_degrees))

   -- Detect bump based on either:
   -- 1. Deviation from intended direction (>20°) - wall sliding
   -- 2. Sharp path change (>30°) - corner collision
   if angle_degrees > 20 or path_angle_degrees > 30 then
      bump.last_bump_tick = this_tick
      notify_bump_callbacks(pindex)
   else
      logger:debug("No bump detected")
   end
end

---Tells the bump callbacks when the player is giving walking input but not moving at all
---@param pindex number
---@param this_tick number
function mod.check_stuck(pindex, this_tick)
   local bump = bump_storage[pindex]

   -- Check cooldown (60 ticks = 1 second) - longer than bump to avoid overlap
   if this_tick - bump.last_stuck_tick < 60 then return end

   -- Get movement history
   local reader = MovementHistory.get_movement_history_reader(pindex)

   -- Check if position is unchanged for last 3 walking entries
   -- This detects being completely stuck against an obstacle
   local stuck = true
   local first_pos = nil
   local walking_count = 0

   for i = 0, 2 do
      local entry = reader:get(i)
      if entry and entry.kind == MovementHistory.MOVEMENT_KINDS.WALKING then
         walking_count = walking_count + 1
         if not first_pos then
            first_pos = entry.position
            logger:debug(string.format("First pos: (%.2f,%.2f)", first_pos.x, first_pos.y))
         elseif first_pos.x ~= entry.position.x or first_pos.y ~= entry.position.y then
            stuck = false
            logger:debug(
               string.format(
                  "Position changed: (%.2f,%.2f) != (%.2f,%.2f)",
                  entry.position.x,
                  entry.position.y,
                  first_pos.x,
                  first_pos.y
               )
            )
            break
         end
      end
   end

   logger:debug(string.format("Walking count: %d, stuck: %s", walking_count, stuck and "true" or "false"))

   -- Need at least 3 walking entries all at same position to confirm stuck
   -- This means the player has been trying to walk for 3+ ticks but hasn't moved
   if stuck and walking_count >= 3 then
      bump.last_stuck_tick = this_tick
      notify_bump_callbacks(pindex)
   end
end

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
