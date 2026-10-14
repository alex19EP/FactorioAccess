--What the FactorioAccess native DLL knows of the game that the Lua API cannot read, such as the
--build direction and the drag in progress. The DLL owns the cursor (scripts/cursor.lua); the game's
--own cursor follows it, so vanilla hover selection, building, mining and the selection tools act
--there; helper-scripts/factorio-access-keys.ps1 gives every mouse button control a key.
--fa_native exists only on clients that run the DLL, and its calls change nothing in the game.
local mod = {}

---@class fa.Native
---@field cursor_mode fun(player_index: integer, vanilla: boolean, keys: boolean, anchored: boolean, rulers: integer[], next_machine_sound: boolean)
---@field cursor_jump fun(player_index: integer, x: number, y: number)
---@field view_entered fun(player_index: integer, hand: LocalisedString?)
---@field view_hand fun(player_index: integer, hand: LocalisedString?): boolean
---@field detach_build fun(player_index: integer)
---@field cursor_step fun(player_index: integer, x: number, y: number): ("read"|"moved"|"edge"|"skipped")?, integer?, boolean?
---@field cursor_read fun(player_index: integer, x: number, y: number, prefix: LocalisedString?)
---@field entity_read fun(player_index: integer, entity: LuaEntity, prefix: LocalisedString?, suffix: LocalisedString?)
---@field cursor_position fun(player_index: integer): integer?, integer?
---@field speak fun(player_index: integer, message: LocalisedString)
---@field next_part fun(player_index: integer, direction: integer)
---@field build_direction fun(player_index: integer): defines.direction?
---@field held_build fun(player_index: integer): fa.NativeHeldBuild?
---@field drag_build fun(player_index: integer): fa.NativeDragBuild?
---@field walking_step fun(player_index: integer): ("full"|"partial"|"none")?, integer?
---@field open_selected_info fun(player_index: integer)
---@field audio fun(player_index: integer, command: fa.LauncherAudio.Command)
---@field rail_route fun(player_index: integer): fa.NativeRailRoute?
---@field scanner_current_category fun(player_index: integer): string?

---@class fa.NativeRailPoint
---@field x number Tiles
---@field y number Tiles
---@field direction integer The way the route leads on from here, 0 to 15
---@field elevated boolean

---@class fa.NativeRailPiece: fa.NativeRailPoint The rail's end the route leads on from
---@field type string The rail's prototype type, such as "straight-rail", "curved-rail-a" or "rail-ramp"

---@class fa.NativeRailRoute
---@field start fa.NativeRailPoint
---@field pieces fa.NativeRailPiece[] In order from the start; empty when there is no route

---@class fa.NativeHeldBuild
---@field blueprint boolean
---@field direction defines.direction A blueprint's rotation
---@field width integer Tiles east to west, as built
---@field height integer Tiles north to south, as built
---@field flippable boolean An entity: whether the flip keys change it
---@field mirrored boolean An entity that flips by mirroring: whether it is mirrored
---@field flip_horizontal boolean A blueprint: flipped east to west on the map
---@field flip_vertical boolean A blueprint: flipped north to south on the map

---@class fa.NativeDragBuild
---@field turn_pending boolean A belt drag turns at the cursor's next step off its line
---@field turns integer Turns the game has made in belt drags, counting on across drags

---@type fa.Native?
local native = rawget(_G, "fa_native")

---Moves to the next part of the screen the DLL reads, such as the quickbar, or back with a
---negative direction.
---@param pindex integer
---@param direction integer
function mod.next_part(pindex, direction)
   if native then native.next_part(pindex, direction) end
end

---Opens the game's own info panel for the entity the cursor points at, or in the map editor the tile
---when there is none, for the DLL to read a line at a time.
---@param pindex integer
function mod.open_selected_info(pindex)
   if native then native.open_selected_info(pindex) end
end

---The direction the game builds the item in hand in, already turned by a rotate key the mod is
---handling. Only this client knows it: speak it, never change the game by it. Handlers of custom
---inputs that change the game use event.cursor_direction instead. A blueprint has its own rotation,
---which this is not.
---@param pindex integer
---@return defines.direction?
function mod.build_direction(pindex)
   return native and native.build_direction(pindex)
end

---The detach key with a building or blueprint in hand: pins it where it is and frees the cursor to
---explore around it, or re-attaches it. Only this client knows the pin; the build input the game
---sends carries its position to every client.
---@param pindex integer
function mod.toggle_detach(pindex)
   if native then native.detach_build(pindex) end
end

---The entity or blueprint in hand as the game builds it, already turned or flipped by a rotate or
---flip key the mod is handling; nil for anything else in hand. Only this client knows it: speak it,
---never change the game by it.
---@param pindex integer
---@return fa.NativeHeldBuild?
function mod.held_build(pindex)
   return native and native.held_build(pindex)
end

---The game's drag building while the build key is held, from the first build to the release; nil
---otherwise. Rotate during a belt drag does not turn the belt in hand: the game turns the line at
---the cursor's next step off it. Only this client knows it: speak it, never change the game by it.
---@param pindex integer
---@return fa.NativeDragBuild?
function mod.drag_build(pindex)
   return native and native.drag_build(pindex)
end

---The route the game's rail planner draws while the player plans, rail by rail; nil when not
---planning. Only this client knows it: speak it, never change the game by it.
---@param pindex integer
---@return fa.NativeRailRoute?
function mod.rail_route(pindex)
   return native and native.rail_route(pindex)
end

return mod
