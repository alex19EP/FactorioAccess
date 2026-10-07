--Hands the FA cursor to the FactorioAccess native DLL when it is loaded. The game's own cursor
--then follows the FA cursor, so vanilla hover selection, building, mining and the selection tools
--act there; helper-scripts/bind-mouse-keys.ps1 gives every mouse button control a key.
--fa_native exists only on clients that run the DLL, and its calls change nothing in the game.
local VanillaMode = require("scripts.vanilla-mode")
local Viewpoint = require("scripts.viewpoint")

local mod = {}

---@class fa.Native
---@field set_cursor fun(player_index: integer, x: number, y: number)
---@field release_cursor fun(player_index: integer)
---@field speak fun(player_index: integer, message: LocalisedString)
---@field next_part fun(player_index: integer, direction: integer)
---@field build_direction fun(player_index: integer): defines.direction?
---@field entity_views_begin fun(player_index: integer, unit_number: integer)
---@field entity_view fun(player_index: integer, title: LocalisedString)
---@field entity_view_column fun(player_index: integer, title: LocalisedString, ...: LocalisedString)
---@field entity_views_end fun(player_index: integer)

---@type fa.Native?
local native = rawget(_G, "fa_native")

---@return boolean
function mod.is_loaded()
   return native ~= nil
end

---Moves to the next part of the screen the DLL reads, such as the quickbar, or back with a
---negative direction.
---@param pindex integer
---@param direction integer
function mod.next_part(pindex, direction)
   if native then native.next_part(pindex, direction) end
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

---Called every tick. Reports each cursor as the centre of its tile.
function mod.on_tick()
   if not native then return end
   for _, p in pairs(game.connected_players) do
      local pindex = p.index
      if VanillaMode.is_enabled(pindex) then
         native.release_cursor(pindex)
      else
         local position = Viewpoint.get_viewpoint(pindex):get_cursor_pos()
         native.set_cursor(pindex, position.x + 0.5, position.y + 0.5)
      end
   end
end

return mod
