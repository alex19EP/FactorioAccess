--[[
The cursor: the tile the blind player points at.

The native DLL owns it (native/src/cursor.h). The game reads it wherever it reads the mouse, so every
mod key's event carries it as cursor_position, the same on every peer. The movement keys move it in
the DLL as the game reads them, with no wait for the server; the DLL also keeps it on the screen in
the character view and keeps the remote view's camera on it.

The mod keeps no cursor of its own. While a key's handlers run, mod.tile gives the tile of that
event's cursor_position, the same on every peer, so code that changes the game may act there. A jump
the mod makes (mod.jump) asks the DLL to move there and moves that tile with it, so the rest of the
handler sees where the cursor went. Code that runs outside a key event names its tile with mod.at.
]]
local mod = {}

---@type fa.Native?
local native = rawget(_G, "fa_native")

-- The cursor's tile while each player's key event, or a mod.at call, is being handled. Never kept
-- past it.
---@type table<integer, fa.Tile?>
local tiles = {}

---@param position MapPosition
---@return fa.Tile
local function tile_of(position)
   ---@cast position MapPosition.struct
   return { x = math.floor(position.x), y = math.floor(position.y) }
end

---Begins a custom input's handling: its cursor_position is the cursor.
---@param event EventData.CustomInputEvent
---@param pindex integer
function mod.begin_event(event, pindex)
   tiles[pindex] = tile_of(event.cursor_position)
end

---Ends a custom input's handling.
---@param pindex integer
function mod.end_event(pindex)
   tiles[pindex] = nil
end

---The cursor's tile while a key event or a mod.at call is handled.
---@param pindex integer
---@return fa.Tile
function mod.tile(pindex)
   local tile = assert(tiles[pindex], "the cursor is known only while a key event is handled")
   return { x = tile.x, y = tile.y }
end

---Whether the cursor's tile is known here: while a key event or a mod.at call is handled.
---@param pindex integer
---@return boolean
function mod.known(pindex)
   return tiles[pindex] ~= nil
end

---Runs `fn` with the cursor on the tile holding `position`, for code outside a key event that knows
---where the cursor is (a jump it made itself).
---@param pindex integer
---@param position MapPosition
---@param fn fun()
function mod.at(pindex, position, fn)
   local outer = tiles[pindex]
   tiles[pindex] = tile_of(position)
   fn()
   tiles[pindex] = outer
end

---Moves the cursor to the tile holding `position`. Off the screen in the character view, the DLL
---opens remote view there.
---@param pindex integer
---@param position MapPosition
function mod.jump(pindex, position)
   ---@cast position MapPosition.struct
   if tiles[pindex] then tiles[pindex] = tile_of(position) end
   if native then native.cursor_jump(pindex, position.x, position.y) end
end

---What the DLL did with the movement or skip key whose event is being handled: "read" when it moved
---there and read the tile itself (native/src/reader.h), "moved" when it moved there unread (a cell
---of the full map, a step of a drag build), "edge" when it stayed at the screen's edge, "skipped"
---when a skip key took it there and the DLL read it, with how many tiles it skipped (-1 when it
---found no change within its limit), or nil. Ctrl with a movement key, which moves by the size of
---the building in hand, gives "read". With "read", third, whether it went from an entity bigger than
---a tile onto the next of its kind, as along a row of assemblers. Only this client knows it: speak
---by it, never change the game by it.
---@param pindex integer
---@param event EventData.CustomInputEvent
---@return ("read"|"moved"|"edge"|"skipped")?, integer?, boolean?
function mod.step(pindex, event)
   if not native then return nil end
   local position = event.cursor_position --[[@as MapPosition.struct]]
   return native.cursor_step(pindex, position.x, position.y)
end

---The DLL reads the cursor's tile as a movement key's step reads it (native/src/reader.h), after
---`prefix` when one is given: for the mod's own jumps and announcements.
---@param pindex integer
---@param prefix LocalisedString?
function mod.read(pindex, prefix)
   local tile = mod.tile(pindex)
   if native then native.cursor_read(pindex, tile.x, tile.y, prefix) end
end

---Tells the DLL, every tick, what the movement and skip keys do now.
---@param pindex integer
---@param vanilla boolean The mouse is the cursor
---@param keys boolean The movement and skip keys move the cursor
---@param anchored boolean The cursor goes back to the character when the screen leaves it
---@param rulers integer[] The tiles of the player's audio rulers as x1, y1, x2, y2..., where skips stop
---@param next_machine_sound boolean The DLL's own screens sound the next machine of a row, as a step does
function mod.report_mode(pindex, vanilla, keys, anchored, rulers, next_machine_sound)
   if native then native.cursor_mode(pindex, vanilla, keys, anchored, rulers, next_machine_sound) end
end

return mod
