--[[
The scanner.

The list lives in the native DLL (native/src/scanner.h), on the client of the player who scans:
nothing of it is in storage, and other clients have none. The DLL keeps it up to date by itself,
asking each tick for a refresh when one is due; the refresh key lists the surface again at once.

Moving through the list only speaks; the cursor stays put. The repeat key (Home) moves the cursor
and the selection onto the current entry, which must change the game alike on every client. The DLL
does the move when the scanner key reaches the game, and the game's cursor reads as the entry's
position while the game turns the key into its custom input event, so every client gets that
position as event.cursor_position. The Home handler lands on it, the same everywhere, and only then
asks the DLL what to say, which is speech alone.
]]
local EntitySelection = require("scripts.entity-selection")
local Extras = require("scripts.scanner.extras")
local FaUtils = require("scripts.fa-utils")
local Localising = require("scripts.localising")
local ScannerConsts = require("scripts.scanner.scanner-consts")
local Readout = require("scripts.scanner.readout")
local UiRouter = require("scripts.ui.router")
local Viewpoint = require("scripts.viewpoint")
local Speech = require("scripts.speech")

local native = rawget(_G, "fa_native")

local mod = {}

-- Entities standing exactly at `position`, in the order the game finds them.
---@param surface LuaSurface
---@param position MapPosition
---@return LuaEntity[]
local function entities_at(surface, position)
   return surface.find_entities_filtered({ position = position, radius = 1 / 512 })
end

---@param pindex integer
local function play_edge(pindex)
   game.get_player(pindex).play_sound({ path = "inventory-edge" })
end

---@param pindex integer
---@param category string
local function speak_nothing(pindex, category)
   Speech.speak(pindex, { "fa.scanner-nothing-in-category", { "fa.scanner-category-" .. category } })
end

-- The tile a scanner key at `position` points the cursor at: the northwest corner of the entity
-- standing there, or the position itself.
---@param pindex integer
---@param position MapPosition
---@return fa.Point
local function target_of(pindex, position)
   local landed = entities_at(game.get_player(pindex).surface, position)[1]
   local target = landed and FaUtils.get_ent_northwest_corner_position(landed) or position
   return { x = math.floor(target.x), y = math.floor(target.y) }
end

-- Moves the cursor to the target and selects what is there. This changes the game, so it reads
-- only the event and the game, never the DLL: every client does the same.
---@param pindex integer
---@param target fa.Point
local function land(pindex, target)
   local player = game.get_player(pindex)
   ---@cast player LuaPlayer
   EntitySelection.reset_entity_index(pindex)
   Viewpoint.get_viewpoint(pindex):set_cursor_pos(target)
   local selected = EntitySelection.get_first_ent_at_tile(pindex)
   if selected then player.selected = selected end
end

-- Says what a scanner key reached. Speech only: this client's DLL knows the list.
---@param pindex integer
---@param event EventData.CustomInputEvent
---@param target fa.Point where the entry's distance is measured to
local function announce(pindex, event, target)
   if not native then return end
   local position = event.cursor_position
   -- Nothing when the DLL left the key alone, as it does while a game window is open.
   local entry = native.scanner_entry(pindex, position.x, position.y)
   if not entry then return end
   if entry.edge then play_edge(pindex) end
   if entry.empty then
      speak_nothing(pindex, entry.category)
      return
   end

   local player = game.get_player(pindex)
   ---@cast player LuaPlayer
   local readout
   if entry.kind == "forest" then
      readout = { "fa.scanner-forest", entry.trees }
   elseif entry.kind == "patch" then
      readout = entry.text
   elseif entry.kind == "water" then
      readout = { "fa.scanner-water", entry.width, entry.height }
   elseif entry.kind == "ice" then
      readout = { "fa.scanner-iceberg", entry.width, entry.height }
   elseif entry.kind == "extra" then
      readout = Extras.readout(pindex, entry.generation, entry.extra)
      if not readout then return end
   else
      local candidates = entities_at(player.surface, position)
      local entity = candidates[1]
      for _, candidate in ipairs(candidates) do
         if candidate.name == entry.prototype then
            entity = candidate
            break
         end
      end
      readout = entity and Readout.of(pindex, entity)
         or Localising.get_localised_name_with_fallback(prototypes.entity[entry.prototype])
   end

   -- In remote view distances are from where the scan was sorted, such as the place a map search
   -- jumped to
   local from = player.position
   if player.controller_type == defines.controllers.remote then from = { x = entry.origin_x, y = entry.origin_y } end
   Speech.speak(pindex, {
      "fa.scanner-full-presentation",
      readout,
      FaUtils.dir_dist_locale(from, target),
      tostring(entry.index),
      tostring(entry.count),
   })
end

-- Each player's latest collection of extras, on that player's client only, as the DLL's lists are.
---@type table<integer, integer>
local generations = {}

-- What the DLL lists the player's surface from: where the player is, and the mod's own extras.
---@param player LuaPlayer
---@param fields table the request's other fields
local function refresh(player, fields)
   local generation = (generations[player.index] or 0) + 1
   generations[player.index] = generation
   fields.surface = player.surface.index
   fields.x = player.position.x
   fields.y = player.position.y
   fields.water = ScannerConsts.WATER_PROTOS
   fields.ice = script.feature_flags.space_travel and ScannerConsts.ICEBERG_PROTOS or nil
   fields.extras = Extras.collect(player, generation)
   fields.generation = generation
   native.scanner_refresh(player.index, fields)
end

-- Lists the player's surface again at once, from where the player is.
---@param pindex number
---@param direction_filter defines.direction?
function mod.do_refresh(pindex, direction_filter)
   local player = game.get_player(pindex)
   ---@cast player LuaPlayer
   player.play_sound({ path = "scanner-pulse" })
   if native then refresh(player, { direction = direction_filter }) end
   if direction_filter then
      Speech.speak(pindex, { "fa.scanner-refreshed-directional", FaUtils.direction_lookup(direction_filter) })
   else
      Speech.speak(pindex, { "fa.scanner-refreshed" })
   end
end

-- A category key: the DLL moved between categories, which moves nothing in the game.
---@param pindex integer
function mod.move_category(pindex)
   if not native then return end
   local category = native.scanner_category(pindex)
   if not category then return end
   if category.edge then play_edge(pindex) end
   Speech.speak(pindex, { "fa.scanner-category-" .. category.category })
end

-- A subcategory or entry key: says what the entry is and leaves the cursor where it is.
---@param pindex integer
---@param event EventData.CustomInputEvent
function mod.move(pindex, event)
   announce(pindex, event, target_of(pindex, event.cursor_position))
end

-- The repeat key: lands on the entry wherever it is, opening remote view when it is off the screen,
-- then says what is there.
---@param pindex integer
---@param event EventData.CustomInputEvent
function mod.jump(pindex, event)
   local target = target_of(pindex, event.cursor_position)
   land(pindex, target)
   announce(pindex, event, target)
end

-- The mod's own UIs take the scanner keys while open; the DLL must leave those keys alone then.
-- The DLL also keeps its list up to date on its own, starting a refresh when it asks for one; in
-- remote view that keeps the list's origin, which the camera leaves as Home jumps to entries.
function mod.on_tick()
   if not native then return end
   for _, player in pairs(game.connected_players) do
      native.scanner_mod_ui(player.index, UiRouter.get_router(player.index):is_ui_open())
      if native.scanner_tick(player.index, player.surface.index) then
         refresh(player, {
            automatic = true,
            keep_origin = player.controller_type == defines.controllers.remote,
         })
      end
   end
end

return mod
