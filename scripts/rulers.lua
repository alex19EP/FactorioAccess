--[[
Audio rulers.

A ruler is a cross through a tile: the tile's row and column, without end. When the cursor or the
character comes onto a new tile in line with one of the player's rulers, a tone plays: a low one a
tile off the line, a high one on it, a click on the ruler's own tile. The DLL's skips stop where
they come into line with more of the player's rulers than the tile before (native/src/skiprules.h).

Rulers are placed with the audio ruler tool (data/audio-ruler.lua), which the shortcut bar and
Ctrl+Alt+B put in hand. As with Kruise Kontrol's spaghetti planner, the tool is held only for its
clicks: a click puts a ruler on the clicked tile, a Shift+click removes the nearest ruler in line
with it. A player may have any number of rulers, each on the surface it was placed on.

Each ruler is drawn on the ground, REACH tiles along each arm, for every player to see, in its
owner's colour. Where the character crosses a line further out, a short stretch of it shows for a
moment.
]]
local Cursor = require("scripts.cursor")
local MovementHistory = require("scripts.movement-history")
local StorageManager = require("scripts.storage-manager")
local VanillaMode = require("scripts.vanilla-mode")

local mod = {}

mod.TOOL = "fa-audio-ruler"

-- How far each arm is drawn from the ruler's tile.
local REACH = 100

-- How far either way of the character a line crossed beyond REACH is drawn, and for how long.
local REVEAL_REACH = 10
local REVEAL_TICKS = 180

-- Line width in pixels, 32 to the tile.
local LINE_WIDTH = 3

---@class fa.Ruler
---@field surface_index integer
---@field x integer
---@field y integer
---@field drawings LuaRenderObject[]

---@class fa.RulerState
---@field rulers fa.Ruler[]
---@field give_back_tick integer? The tool was taken from the hand after a click, to go back on this tick

---@type table<integer, fa.RulerState>
local ruler_storage = StorageManager.declare_storage_module("rulers", {
   rulers = {},
}, {
   ephemeral_state_version = 2,
})

-- Saves from before the tool hold a ruler handle with this metatable until the version above clears
-- it.
script.register_metatable("RulerHandle", {})

-- The cursor tile each player's rulers last sounded for. Only the client with the DLL knows the
-- cursor, so this stays out of storage: it only decides when to play a sound.
---@type table<integer, fa.Tile?>
local last_cursor_tiles = {}

-- For the DLL's skips, every tick: each player's rulers on one surface as x1, y1, x2, y2... Rebuilt
-- when the rulers or the surface change.
---@type table<integer, { surface_index: integer, tiles: integer[] }>
local tile_lists = {}

---@enum fa.RulerAlignment
local ALIGNMENT = {
   NONE = 0,
   -- Within a tile of a line.
   CLOSE = 1,
   -- On a line.
   ON_LINE = 2,
   -- On the ruler's own tile.
   AT_RULER = 3,
}

---@type table<fa.RulerAlignment, PlaySoundSpecification?>
local ALIGNMENT_SOUNDS = {
   [ALIGNMENT.AT_RULER] = { path = "audio-ruler-at-definition" },
   [ALIGNMENT.ON_LINE] = { path = "audio-ruler-aligned" },
   [ALIGNMENT.CLOSE] = { path = "audio-ruler-close" },
}

---@param x integer
---@param y integer
---@param ruler fa.Ruler
---@return fa.RulerAlignment
local function alignment_with(x, y, ruler)
   local dx = math.abs(ruler.x - x)
   local dy = math.abs(ruler.y - y)
   if dx > 1 and dy > 1 then return ALIGNMENT.NONE end
   if dx == 0 and dy == 0 then return ALIGNMENT.AT_RULER end
   if dx == 0 or dy == 0 then return ALIGNMENT.ON_LINE end
   return ALIGNMENT.CLOSE
end

---The strongest alignment of the tile with any of the player's rulers on the surface.
---@param pindex integer
---@param surface_index integer
---@param x integer
---@param y integer
---@return fa.RulerAlignment
local function best_alignment(pindex, surface_index, x, y)
   local best = ALIGNMENT.NONE
   for _, ruler in ipairs(ruler_storage[pindex].rulers) do
      if ruler.surface_index == surface_index then
         local alignment = alignment_with(x, y, ruler)
         if alignment > best then best = alignment end
      end
   end
   return best
end

---@param pindex integer
---@param surface_index integer
---@param x integer
---@param y integer
local function sound_tile(pindex, surface_index, x, y)
   local sound = ALIGNMENT_SOUNDS[best_alignment(pindex, surface_index, x, y)]
   if sound then game.get_player(pindex).play_sound(sound) end
end

---@param player LuaPlayer
---@return Color
local function line_colour(player)
   local c = player.color
   -- Rendering takes premultiplied alpha.
   local a = 0.8
   return { r = c.r * a, g = c.g * a, b = c.b * a, a = a }
end

---@param player LuaPlayer
---@param ruler fa.Ruler
local function draw(player, ruler)
   local colour = line_colour(player)
   local cx = ruler.x + 0.5
   local cy = ruler.y + 0.5
   local function line(from, to)
      return rendering.draw_line({
         color = colour,
         width = LINE_WIDTH,
         from = from,
         to = to,
         surface = ruler.surface_index,
         draw_on_ground = true,
      })
   end
   ruler.drawings = {
      line({ cx - REACH, cy }, { cx + REACH, cy }),
      line({ cx, cy - REACH }, { cx, cy + REACH }),
      rendering.draw_rectangle({
         color = colour,
         width = LINE_WIDTH,
         filled = false,
         left_top = { ruler.x, ruler.y },
         right_bottom = { ruler.x + 1, ruler.y + 1 },
         surface = ruler.surface_index,
         draw_on_ground = true,
      }),
   }
end

---@param ruler fa.Ruler
local function erase(ruler)
   for _, drawing in ipairs(ruler.drawings) do
      if drawing.valid then drawing.destroy() end
   end
end

---Draws a short stretch of each line the character's tile lies on beyond the drawn arms.
---@param pindex integer
---@param surface_index integer
---@param x integer
---@param y integer
local function reveal_far_lines(pindex, surface_index, x, y)
   local player = assert(game.get_player(pindex))
   for _, ruler in ipairs(ruler_storage[pindex].rulers) do
      if ruler.surface_index == surface_index then
         ---@type MapPosition?, MapPosition?
         local from, to
         if x == ruler.x and math.abs(y - ruler.y) > REACH then
            from = { x + 0.5, y + 0.5 - REVEAL_REACH }
            to = { x + 0.5, y + 0.5 + REVEAL_REACH }
         elseif y == ruler.y and math.abs(x - ruler.x) > REACH then
            from = { x + 0.5 - REVEAL_REACH, y + 0.5 }
            to = { x + 0.5 + REVEAL_REACH, y + 0.5 }
         end
         if from and to then
            rendering.draw_line({
               color = line_colour(player),
               width = LINE_WIDTH,
               from = from,
               to = to,
               surface = surface_index,
               draw_on_ground = true,
               time_to_live = REVEAL_TICKS,
            })
         end
      end
   end
end

---@param player LuaPlayer
---@param text LocalisedString
local function tell(player, text)
   player.create_local_flying_text({ text = text, create_at_cursor = true })
end

---@param player LuaPlayer
---@return boolean
local function holds_tool(player)
   local stack = player.cursor_stack
   return stack ~= nil and stack.valid_for_read and stack.name == mod.TOOL
end

-- A click with the tool also starts the game's selection, which the DLL holds open until the next
-- press, as a gamepad's. An empty hand ends it with no action, but only if the hand is still empty
-- when the game next looks, before the next tick, so the tool goes back then: the click's own tick
-- runs on_tick after its input events. The hand's handler ignores the swap (mod.tool_away).
---@param player LuaPlayer
local function end_selection(player)
   player.clear_cursor()
   ruler_storage[player.index].give_back_tick = game.tick + 1
end

---Whether the tool is out of the player's hand for a click and goes back next tick: the hand
---changes then are not the player's own.
---@param pindex integer
---@return boolean
function mod.tool_away(pindex)
   return ruler_storage[pindex].give_back_tick ~= nil
end

---Puts the audio ruler tool in the player's hand, from the shortcut bar or Ctrl+Alt+B.
---@param pindex integer
function mod.give_tool(pindex)
   local player = assert(game.get_player(pindex))
   player.clear_cursor()
   local stack = player.cursor_stack
   -- A hand the inventory had no room for stays as it is.
   if stack.valid_for_read then return end
   stack.set_stack({ name = mod.TOOL })
end

---A click with the tool: a ruler on the clicked tile.
---@param pindex integer
function mod.place(pindex)
   local player = assert(game.get_player(pindex))
   if not holds_tool(player) then return end
   end_selection(player)
   local tile = Cursor.tile(pindex)
   local surface_index = player.surface_index
   local rulers = ruler_storage[pindex].rulers
   for _, ruler in ipairs(rulers) do
      if ruler.surface_index == surface_index and ruler.x == tile.x and ruler.y == tile.y then
         tell(player, { "fa.ruler-exists", tile.x, tile.y })
         return
      end
   end
   ---@type fa.Ruler
   local ruler = { surface_index = surface_index, x = tile.x, y = tile.y, drawings = {} }
   draw(player, ruler)
   table.insert(rulers, ruler)
   tile_lists[pindex] = nil
   tell(player, { "fa.ruler-placed", tile.x, tile.y })
end

---A Shift+click with the tool: removes the nearest ruler whose row or column holds the clicked tile.
---@param pindex integer
function mod.remove(pindex)
   local player = assert(game.get_player(pindex))
   if not holds_tool(player) then return end
   end_selection(player)
   local tile = Cursor.tile(pindex)
   local surface_index = player.surface_index
   local rulers = ruler_storage[pindex].rulers
   local nearest, nearest_distance
   for i, ruler in ipairs(rulers) do
      if ruler.surface_index == surface_index and (ruler.x == tile.x or ruler.y == tile.y) then
         local distance = math.abs(ruler.x - tile.x) + math.abs(ruler.y - tile.y)
         if not nearest or distance < nearest_distance then
            nearest, nearest_distance = i, distance
         end
      end
   end
   if not nearest then
      tell(player, { "fa.ruler-none-in-line" })
      return
   end
   local ruler = table.remove(rulers, nearest)
   erase(ruler)
   tile_lists[pindex] = nil
   tell(player, { "fa.ruler-removed", ruler.x, ruler.y })
end

---The player's rulers on the surface the player views, as x1, y1, x2, y2...: the DLL's skips stop
---by them. The same table each tick until the rulers or the surface change.
---@param pindex integer
---@return integer[]
function mod.ruler_tiles(pindex)
   local surface_index = game.get_player(pindex).surface_index
   local list = tile_lists[pindex]
   if list and list.surface_index == surface_index then return list.tiles end
   local tiles = {}
   for _, ruler in ipairs(ruler_storage[pindex].rulers) do
      if ruler.surface_index == surface_index then
         table.insert(tiles, ruler.x)
         table.insert(tiles, ruler.y)
      end
   end
   tile_lists[pindex] = { surface_index = surface_index, tiles = tiles }
   return tiles
end

---Plays the tones for the tiles the character and the cursor came onto this tick.
---@param pindex integer
local function update_player(pindex)
   if not next(ruler_storage[pindex].rulers) then return end
   -- Storage outlives a player removed from the game.
   local player = assert(game.get_player(pindex))
   if not player then return end

   local reader = MovementHistory.get_movement_history_reader(pindex)
   local current_entry = reader:get(0)
   local prev_entry = reader:get(1)
   if
      current_entry
      and prev_entry
      and (
         current_entry.kind == MovementHistory.MOVEMENT_KINDS.WALKING
         or current_entry.kind == MovementHistory.MOVEMENT_KINDS.DRIVING
      )
   then
      local x = math.floor(current_entry.position.x)
      local y = math.floor(current_entry.position.y)
      if x ~= math.floor(prev_entry.position.x) or y ~= math.floor(prev_entry.position.y) then
         sound_tile(pindex, player.physical_surface_index, x, y)
         reveal_far_lines(pindex, player.physical_surface_index, x, y)
      end
   end

   -- Only this client knows the cursor: sounds only from here on.
   local cursor_tile = Cursor.speech_tile(pindex)
   if not cursor_tile then return end
   local last = last_cursor_tiles[pindex]
   if not last or cursor_tile.x ~= last.x or cursor_tile.y ~= last.y then
      sound_tile(pindex, player.surface_index, cursor_tile.x, cursor_tile.y)
      last_cursor_tiles[pindex] = cursor_tile
   end
end

---Puts the tool back in the hand it left for a click.
---@param pindex integer
local function give_back(pindex)
   ruler_storage[pindex].give_back_tick = nil
   -- Storage outlives a player removed from the game.
   local player = assert(game.get_player(pindex))
   if player and player.cursor_stack and not player.cursor_stack.valid_for_read then
      player.cursor_stack.set_stack({ name = mod.TOOL })
   end
end

---Called from on_tick.
function mod.update_all_players()
   for pindex, _ in pairs(storage.players) do
      local state = ruler_storage[pindex]
      if state.give_back_tick and game.tick >= state.give_back_tick then give_back(pindex) end
      if not VanillaMode.is_enabled(pindex) then update_player(pindex) end
   end
end

---Before each of the player's keys. A paused map, such as the map editor's in single player, runs
---no tick, so the tool goes back at the next key instead: the game has looked at the empty hand in
---the frames between.
---@param pindex integer
function mod.on_key(pindex)
   if ruler_storage[pindex].give_back_tick and game.tick_paused then give_back(pindex) end
end

return mod
