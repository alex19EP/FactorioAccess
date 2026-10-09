--[[
The scanner.

The list lives in the native DLL (native/src/scanner.h), on the client of the player who scans:
nothing of it is in storage, and other clients have none. Refreshing asks the DLL to list the
surface again.

Moving through the list must still change the game alike on every client, since it moves the cursor
and the selection. The DLL does the move when the scanner key reaches the game, and the game's
cursor reads as the entry's position while the game turns the key into its custom input event, so
every client gets that position as event.cursor_position. The handlers here land on it, the same
everywhere, and only then ask the DLL what to say, which is speech alone.
]]
local EntitySelection = require("scripts.entity-selection")
local FaInfo = require("scripts.fa-info")
local FaUtils = require("scripts.fa-utils")
local Localising = require("scripts.localising")
local ScannerConsts = require("scripts.scanner.scanner-consts")
local SurfaceScanner = require("scripts.scanner.surface-scanner")
local UiRouter = require("scripts.ui.router")
local Viewpoint = require("scripts.viewpoint")
local WorkQueue = require("scripts.work-queue")
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

-- Moves the cursor to where a scanner key took it and selects what is there. This changes the game,
-- so it reads only the event and the game, never the DLL: every client does the same.
---@param pindex integer
---@param position MapPosition
---@return LuaEntity? landed the entity the cursor landed on
local function land(pindex, position)
   local player = game.get_player(pindex)
   ---@cast player LuaPlayer
   local vp = Viewpoint.get_viewpoint(pindex)
   local landed = entities_at(player.surface, position)[1]
   EntitySelection.reset_entity_index(pindex)
   vp:set_cursor_pos(landed and FaUtils.get_ent_northwest_corner_position(landed) or position)
   local selected = EntitySelection.get_first_ent_at_tile(pindex)
   if selected then player.selected = selected end
   return landed
end

-- Says what a scanner key landed on. Speech only: this client's DLL knows the list.
---@param pindex integer
---@param event EventData.CustomInputEvent
local function announce(pindex, event)
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
   local candidates = entities_at(player.surface, position)
   local entity = candidates[1]
   for _, candidate in ipairs(candidates) do
      if candidate.name == entry.prototype then
         entity = candidate
         break
      end
   end
   local readout = entity and FaInfo.ent_info(pindex, entity, true)
      or Localising.get_localised_name_with_fallback(prototypes.entity[entry.prototype])

   -- In remote view the camera follows the cursor onto each entry, so distances are from where
   -- the scan was sorted, such as the place a map search jumped to
   local from = player.position
   if player.controller_type == defines.controllers.remote then from = { x = entry.origin_x, y = entry.origin_y } end
   local cursor = Viewpoint.get_viewpoint(pindex):get_cursor_pos()
   Speech.speak(pindex, {
      "fa.scanner-full-presentation",
      readout,
      FaUtils.dir_dist_locale(from, cursor),
      tostring(entry.index),
      tostring(entry.count),
   })
end

-- Lists the player's surface again, from where the player is.
---@param pindex number
---@param direction_filter defines.direction?
function mod.do_refresh(pindex, direction_filter)
   local player = game.get_player(pindex)
   ---@cast player LuaPlayer
   player.play_sound({ path = "scanner-pulse" })
   if native then
      native.scanner_refresh(pindex, {
         surface = player.surface.index,
         x = player.position.x,
         y = player.position.y,
         radius = ScannerConsts.SCANNER_DISTANCE,
         direction = direction_filter,
      })
   end
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

-- A subcategory, entry or repeat key: lands where the key took the cursor, then says what is there.
---@param pindex integer
---@param event EventData.CustomInputEvent
function mod.move(pindex, event)
   land(pindex, event.cursor_position)
   announce(pindex, event)
end

--[[
There is a crash in Factorio.  If we query a surface during a created_effect in
the case that entities are being rapidly created or destroyed, sometimes getting
entities crashes out.  See
https://forums.factorio.com/viewtopic.php?f=7&t=115615&p=619147#p619147

To deal with this we just delay the incoming effect triggers so that the code
that runs doesn't run while the trigger is still going.
]]
---@param args { surface_index: number, entity: LuaEntity }
local function on_new_entity_delayed(args)
   SurfaceScanner.on_new_entity(args.surface_index, args.entity)
end

local new_entity_queue = WorkQueue.declare_work_queue({
   name = "scanner_delayed_new_ents",
   worker_function = on_new_entity_delayed,
   per_tick = 100,
})

-- Called from control.lua whenever control.lua finds out about a new entity.
---@param surface_index number
---@param entity LuaEntity
function mod.on_new_entity(surface_index, entity)
   new_entity_queue:enqueue({ surface_index = surface_index, entity = entity })
end

-- Returns an event handler that extracts an entity from the given field and
-- passes it to the scanner.
---@param field_name string
---@param allow_nil boolean? If true, nil entity is allowed (for optional fields). Default false.
---@return fun(event: table)
function mod.build_new_entity_handler(field_name, allow_nil)
   return function(event)
      local entity = event[field_name]
      if not entity then
         if not allow_nil then error("Expected entity in field '" .. field_name .. "' but got nil") end
         return
      end
      if entity.valid then mod.on_new_entity(entity.surface.index, entity) end
   end
end

function mod.on_entity_destroyed(event)
   SurfaceScanner.on_entity_destroyed(event)
end

function mod.on_new_surface(surface)
   SurfaceScanner.on_new_surface(surface.index)
end

function mod.on_surface_delete(index)
   SurfaceScanner.on_surface_delete(index)
end

-- The mod's own UIs take the scanner keys while open; the DLL must leave those keys alone then.
function mod.on_tick()
   if not native then return end
   for _, player in pairs(game.connected_players) do
      native.scanner_mod_ui(player.index, UiRouter.get_router(player.index):is_ui_open())
   end
end

return mod
