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

---@type fa.Native?
local native = rawget(_G, "fa_native")

---@return boolean
function mod.is_loaded()
   return native ~= nil
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
