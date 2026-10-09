--[[
Vanilla mode makes the mod behave more like standard Factorio for sighted players.
When enabled:
- Gives the game cursor back to the mouse (the DLL hears it every tick)
- Silences TTS output
]]

local StorageManager = require("scripts.storage-manager")

local mod = {}

---@class fa.VanillaModeState
---@field enabled boolean

---@type table<integer, fa.VanillaModeState>
local vanilla_state = StorageManager.declare_storage_module("vanilla_mode", {
   enabled = false,
}, {
   ephemeral_state_version = 1,
})

---@param pindex integer
---@return boolean
function mod.is_enabled(pindex)
   return vanilla_state[pindex].enabled
end

---@param pindex integer
function mod.toggle(pindex)
   local state = vanilla_state[pindex]
   state.enabled = not state.enabled
end

return mod
