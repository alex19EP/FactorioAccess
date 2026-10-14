--[[
Pin arrows: the arrows the game draws at the edge of the screen toward each always visible pin that
is off screen (Pin::draw), as sound.

Every 2 seconds, if the cursor or the camera moved since the last time, each such pin plays the
pin arrow sound one after another, panned toward it, muffled when it is south of the screen's
centre and quieter the farther it is.

The cursor is known only to the client with the DLL, and only that player hears these, so the state
here is this client's own: it plays sounds and never changes the game.
]]
local Extras = require("scripts.scanner.extras")
local LauncherAudio = require("scripts.launcher-audio")
local SoundModel = require("scripts.sound-model")

---@type fa.Native?
local native = rawget(_G, "fa_native")

local mod = {}

local SOUND_FILE = "pin-arrow.wav"
local SOUND_VOLUME = 0.8
-- Quietest a far pin gets, so that every arrow stays audible.
local MIN_GAIN = 0.2

local CHECK_INTERVAL = 120
-- Ticks between two pins' sounds, longer than the sound.
local SPACING = 24

-- The base pixels per tile at zoom 1, as zoom.lua reads the screen.
local BASE_PIXELS_PER_TILE = 32

---@class fa.PinArrows.State
---@field cursor_x integer?
---@field cursor_y integer?
---@field camera_x number?
---@field camera_y number?
---@field queue LuaPin[] pins still to sound from the last check
---@field next_tick integer when the next one sounds

---@type table<integer, fa.PinArrows.State>
local states = {}

---Half the width and height of the screen in tiles.
---@param player LuaPlayer
---@return number, number
local function half_screen(player)
   local scale = player.zoom * BASE_PIXELS_PER_TILE * player.display_density_scale
   local resolution = player.display_resolution
   return resolution.width / scale / 2, resolution.height / scale / 2
end

---Whether the cursor or the camera moved since the last check, remembering where they are now.
---@param player LuaPlayer
---@param state fa.PinArrows.State
---@return boolean
local function moved(player, state)
   local cursor_x, cursor_y = native.cursor_position(player.index)
   local camera = player.position --[[@as MapPosition.struct]]
   local changed = cursor_x ~= state.cursor_x
      or cursor_y ~= state.cursor_y
      or camera.x ~= state.camera_x
      or camera.y ~= state.camera_y
   state.cursor_x, state.cursor_y = cursor_x, cursor_y
   state.camera_x, state.camera_y = camera.x, camera.y
   return changed
end

---The pins the game points an arrow at now: always visible, on the player's surface, off screen.
---@param player LuaPlayer
---@return LuaPin[]
function mod.arrow_pins(player)
   local camera = player.position --[[@as MapPosition.struct]]
   local half_width, half_height = half_screen(player)
   local pins = {}
   for _, pin in ipairs(player.get_pins()) do
      if pin.always_visible and Extras.pin_surface_index(pin) == player.surface_index then
         local center = pin.get_pin_center()
         if center and (math.abs(center.x - camera.x) > half_width or math.abs(center.y - camera.y) > half_height) then
            table.insert(pins, pin)
         end
      end
   end
   return pins
end

---Plays a pin's sound from where it is now, as seen from the screen's centre.
---@param player LuaPlayer
---@param pin LuaPin
local function sound(player, pin)
   local center = pin.get_pin_center()
   if not center then return end
   local camera = player.position --[[@as MapPosition.struct]]
   local dx, dy = center.x - camera.x, center.y - camera.y
   local distance = math.sqrt(dx * dx + dy * dy)
   if distance == 0 then return end
   local half_width = half_screen(player)
   -- Panned by the angle alone: an arrow is at the edge whatever the distance.
   local params = SoundModel.map_relative_position(dx, dy, distance, half_width)
   local gain = math.max(MIN_GAIN, params.gain or 1)
   local builder =
      LauncherAudio.patch("pin-arrow-" .. pin.index):file(SOUND_FILE):volume(SOUND_VOLUME * gain):pan(params.pan)
   SoundModel.apply_lpf(builder, params)
   builder:send(player.index)
end

---@param pindex integer
function mod.on_tick_per_player(pindex)
   if not native then return end
   local player = game.get_player(pindex)
   ---@cast player LuaPlayer
   local state = states[pindex]
   if not state then
      state = { queue = {}, next_tick = 0 }
      states[pindex] = state
      -- Where things are at the start is not a move
      moved(player, state)
   end

   if game.tick % CHECK_INTERVAL == 0 and #state.queue == 0 and moved(player, state) then
      state.queue = mod.arrow_pins(player)
      state.next_tick = game.tick
   end

   if #state.queue > 0 and game.tick >= state.next_tick then
      local pin = table.remove(state.queue, 1)
      if pin.valid then sound(player, pin) end
      state.next_tick = game.tick + SPACING
   end
end

return mod
