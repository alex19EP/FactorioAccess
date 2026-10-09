--[[
What the player looks at, beyond the cursor itself.

The cursor's position is the native DLL's alone (scripts/cursor.lua). This module keeps the cursor's
mode, which every peer shares: whether it is anchored to the character, so that walking carries it a
tile ahead of the character and the DLL brings it back to the character when the screen leaves it
behind.

This module is a leaf: it imports nothing that needs the viewpoint.
]]
local StorageManager = require("scripts.storage-manager")

local mod = {}

---@class fa.viewpoint.ViewpointState
---@field cursor_anchored boolean

---@type table<number, fa.viewpoint.ViewpointState>
local viewpoint_storage = StorageManager.declare_storage_module("viewpoint", {
   cursor_anchored = false,
})

---@class fa.Viewpoint
---@field pindex number
local Viewpoint = {}
local Viewpoint_meta = { __index = Viewpoint }

---@return boolean
function Viewpoint:get_cursor_anchored()
   return viewpoint_storage[self.pindex].cursor_anchored
end

---@param val boolean
function Viewpoint:set_cursor_anchored(val)
   assert(type(val) == "boolean")
   viewpoint_storage[self.pindex].cursor_anchored = val
end

local viewpoint_cache = {}

---@param pindex number
---@return fa.Viewpoint
function mod.get_viewpoint(pindex)
   local cached = viewpoint_cache[pindex]
   if not cached then
      cached = {
         pindex = pindex,
      }
      setmetatable(cached, Viewpoint_meta)
      viewpoint_cache[pindex] = cached
   end
   return viewpoint_cache[pindex]
end

return mod
