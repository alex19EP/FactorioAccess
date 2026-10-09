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
---@type table<integer, fa.Point>
local tiles = {}

---@param position MapPosition
---@return fa.Point
local function tile_of(position)
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
---@return fa.Point
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
   if tiles[pindex] then tiles[pindex] = tile_of(position) end
   if native then native.cursor_jump(pindex, position.x, position.y) end
end

---What the DLL did with the movement key whose event is being handled: "read" when it moved there and
---read the tile itself (native/src/reader.h), "moved" when it moved there unread (a cell of the full
---map, a step of a drag build), "edge" when it stayed at the screen's edge, or nil. Only this client
---knows it: speak by it, never change the game by it.
---@param pindex integer
---@param event EventData.CustomInputEvent
---@return ("read"|"moved"|"edge")?
function mod.step(pindex, event)
   return native and native.cursor_step(pindex, event.cursor_position.x, event.cursor_position.y)
end

---Says the mod's own words for the tile a step read, after the DLL's reading of it; dropped when a
---later step has been read since.
---@param pindex integer
---@param text LocalisedString
function mod.read_more(pindex, text)
   local tile = mod.tile(pindex)
   if native then native.cursor_read_more(pindex, tile.x, tile.y, text) end
end

---The entity the DLL's reading of the cursor's tile described, so the mod's own words are about the
---same one: the game's selection under the mouse there, which the mod's own entity order may not
---pick. Nil when it read the tile without one. Only this client knows it: speak by it, never change
---the game by it.
---@param pindex integer
---@return LuaEntity?
function mod.read_entity(pindex)
   if not native then return nil end
   local tile = mod.tile(pindex)
   local name, x, y = native.cursor_read_entity(pindex, tile.x, tile.y)
   if not name then return nil end
   return game.get_player(pindex).surface.find_entity(name, { x = x, y = y })
end

---The cursor's tile at any time, for sounds and speech only: only this client knows it, so never
---change the game by it. Nil without the DLL.
---@param pindex integer
---@return fa.Point?
function mod.speech_tile(pindex)
   if tiles[pindex] then return mod.tile(pindex) end
   if not native then return nil end
   local x, y = native.cursor_position(pindex)
   if not x then return nil end
   return { x = x, y = y }
end

---Tells the DLL, every tick, what the movement keys do now.
---@param pindex integer
---@param vanilla boolean The mouse is the cursor
---@param keys boolean The movement keys move the cursor
---@param anchored boolean The cursor goes back to the character when the screen leaves it
function mod.report_mode(pindex, vanilla, keys, anchored)
   if native then native.cursor_mode(pindex, vanilla, keys, anchored) end
end

return mod
