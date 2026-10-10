--[[
Walking module - handles cursor announcements during player movement.

Works for the character and for god mode (the sandbox), which walks without a body.

When cursor is anchored:
- Updates cursor to one tile ahead of player on tile border crossing
- Announces entities and unwalkable tiles

When cursor is not anchored:
- Checks for entities/obstacles at cursor position
- Announces entities and unwalkable tiles

Uses movement history to detect:
- Tile border crossings (comparing floored positions)
- Discontinuities (teleports, respawns via generation tracking)
]]

local StorageManager = require("scripts.storage-manager")
local MovementHistory = require("scripts.movement-history")
local Viewpoint = require("scripts.viewpoint")
local Cursor = require("scripts.cursor")
local FaUtils = require("scripts.fa-utils")
local EntitySelection = require("scripts.entity-selection")
local Sounds = require("scripts.ui.sounds")
local Combat = require("scripts.combat")
local VanillaMode = require("scripts.vanilla-mode")

local mod = {}

---@class fa.Walking.State
---@field last_generation number? Last movement history generation we processed

local walking_storage = StorageManager.declare_storage_module("walking", {
   last_generation = nil,
})

---Process walking announcements for a player
---@param pindex number
function mod.process_walking_announcements(pindex)
   local player = game.get_player(pindex)
   if not player then return end
   if VanillaMode.is_enabled(pindex) then return end

   local reader = MovementHistory.get_movement_history_reader(pindex)
   local current_generation = reader:get_generation()
   local vp = Viewpoint.get_viewpoint(pindex)
   local state = walking_storage[pindex]

   -- Detect discontinuities (teleport, respawn, surface change, etc.)
   local had_discontinuity = false
   if state.last_generation ~= current_generation then
      state.last_generation = current_generation
      had_discontinuity = true
   end

   -- Detect tile border crossing
   local current = reader:get(0)
   local previous = reader:get(1)
   local crossed_tile = false

   if not previous then
      -- First movement entry, char teleported or started walking.
      had_discontinuity = true
   elseif current then
      local current_tile = { x = math.floor(current.position.x), y = math.floor(current.position.y) }
      local previous_tile = { x = math.floor(previous.position.x), y = math.floor(previous.position.y) }
      if current_tile.x ~= previous_tile.x or current_tile.y ~= previous_tile.y then crossed_tile = true end
   end

   -- Only process if we crossed a tile. Do not process for discontinuity! Teleport isn't walking.
   if had_discontinuity or not crossed_tile then return end

   -- In combat mode, treat cursor as unanchored (don't update cursor position while walking)
   if vp:get_cursor_anchored() and not Combat.is_combat_mode(pindex) then
      -- ANCHORED MODE: Update cursor ahead and announce if notable
      local direction = current and current.direction or defines.direction.north
      local ahead = FaUtils.to_neighboring_tile(player.physical_position, direction)
      Cursor.jump(pindex, ahead)

      Cursor.at(pindex, ahead, function()
         -- Only announce if there's an entity or the tile is unwalkable. God mode walks through everything.
         local ent = EntitySelection.get_first_ent_at_tile(pindex)
         local blocked = player.character ~= nil
            and not player.surface.can_place_entity({ name = "character", position = Cursor.tile(pindex) })
         if not ((ent ~= nil and ent.valid) or blocked) or player.driving then return end
         if
            ent ~= nil
            and ent.valid
            and (player.character == nil or player.character.unit_number ~= ent.unit_number)
         then
            Sounds.play_close_inventory(pindex)
         end
         Cursor.read(pindex)
      end)
   else
      -- NON-ANCHORED MODE: Don't announce while walking
      -- Cursor stays in place, so no need to read tile on every player movement
   end
end

return mod
