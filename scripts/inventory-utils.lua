--[[
Inventory utility functions: deducting the items a placement costs, and presenting item lists
]]

local F = require("scripts.field-ref")
local FaUtils = require("scripts.fa-utils")
local ItemInfo = require("scripts.item-info")
local Sounds = require("scripts.ui.sounds")
local Speech = require("scripts.speech")
local TH = require("scripts.table-helpers")

local mod = {}

--------------------------------------------------------------------------------
-- Deductor: Aggregate inventory deduction with try-before-commit semantics
--------------------------------------------------------------------------------

---@class fa.InventoryUtils.Deductor
---@field private _player LuaPlayer
---@field private _pending table<string, integer> Pending deductions by item name
local Deductor = {}
mod.Deductor = Deductor
local Deductor_meta = { __index = Deductor }

---Create a new deductor for a player
---@param pindex integer
---@return fa.InventoryUtils.Deductor
function Deductor.new(pindex)
   return setmetatable({
      _player = game.get_player(pindex),
      _pending = {},
   }, Deductor_meta)
end

---Get total available count for an item (inventory + cursor)
---@param name string Item prototype name
---@return integer
---@private
function Deductor:_get_available(name)
   ---@type integer
   local total = self._player.get_main_inventory().get_item_count(name)

   -- Cursor
   local cursor = self._player.cursor_stack
   if cursor and cursor.valid_for_read and cursor.name == name then total = total + cursor.count end

   return total
end

---Try to add a deduction. Only adds if the new total can be satisfied.
---@param name string Item prototype name
---@param count integer Amount to add
---@return boolean success True if the new total is available
function Deductor:try_add(name, count)
   local current = self._pending[name] or 0
   local new_total = current + count
   local available = self:_get_available(name)

   if new_total > available then return false end

   self._pending[name] = new_total
   return true
end

---Commit all pending deductions
---Removes items from inventory/cursor
function Deductor:commit()
   for name, count in pairs(self._pending) do
      local remaining = count

      -- First: deduct from main inventory
      local main_inv = self._player.get_main_inventory()
      local available = main_inv.get_item_count(name)
      if available > 0 then
         local take = math.min(available, remaining)
         remaining = remaining - main_inv.remove({ name = name, count = take })
      end

      -- Second: deduct from cursor
      if remaining > 0 then
         local cursor = self._player.cursor_stack
         if cursor and cursor.valid_for_read and cursor.name == name then
            local take = math.min(cursor.count, remaining)
            cursor.count = cursor.count - take
            remaining = remaining - take
         end
      end

      assert(remaining == 0, "Deductor: commit failed to remove all items for " .. name)
   end

   self._pending = {}
end

---Check if this deductor has any pending deductions
---@return boolean
function Deductor:is_empty()
   return next(self._pending) == nil
end

---Get the pending count for an item
---@param name string Item prototype name
---@return integer
function Deductor:get_pending(name)
   return self._pending[name] or 0
end

---Try to add cost for an entity prototype
---Tries all items_to_place_this options until one works
---@param proto_name string Entity prototype name
---@return boolean success True if cost was added
function Deductor:try_add_entity(proto_name)
   local proto = prototypes.entity[proto_name]
   if not proto then return false end

   local items = proto.items_to_place_this
   if not items or #items == 0 then return true end

   for _, required in ipairs(items) do
      if self:try_add(required.name, required.count) then return true end
   end
   return false
end

---Announce that an entity can't be afforded
---@param pindex integer
---@param proto_name string
local function announce_missing_items(pindex, proto_name)
   local proto = prototypes.entity[proto_name]
   if not proto then
      Sounds.play_cannot_build(pindex)
      Speech.speak(pindex, { "fa.cannot-build" })
      return
   end

   local items = proto.items_to_place_this
   if not items or #items == 0 then return end

   Sounds.play_cannot_build(pindex)
   local needed_list = mod.present_list({ { name = items[1].name, count = items[1].count } }, nil, nil, true)
   Speech.speak(pindex, { "fa.missing-items", needed_list or "" })
end

---Create a deductor for placing a single entity prototype
---@param pindex integer
---@param proto_name string Entity prototype name
---@param silent boolean? If true, don't announce errors
---@return fa.InventoryUtils.Deductor|nil deductor The deductor, or nil if items unavailable
function mod.deductor_to_place(pindex, proto_name, silent)
   local deductor = Deductor.new(pindex)
   if deductor:try_add_entity(proto_name) then return deductor end

   if not silent then announce_missing_items(pindex, proto_name) end
   return nil
end

---Create a deductor for placing multiple entities
---@param pindex integer
---@param proto_names string[] List of entity prototype names
---@param silent boolean? If true, don't announce errors
---@return fa.InventoryUtils.Deductor|nil deductor The deductor, or nil if items unavailable
---@return string|nil missing_proto Name of first entity that couldn't be afforded
function mod.deductor_for_placements(pindex, proto_names, silent)
   local deductor = Deductor.new(pindex)

   for _, proto_name in ipairs(proto_names) do
      if not deductor:try_add_entity(proto_name) then
         if not silent then announce_missing_items(pindex, proto_name) end
         return nil, proto_name
      end
   end

   return deductor, nil
end

-- Present a list like iron plate x1, transport belt legendary x2, ...
---@param list ({ name: string|LuaItemPrototype, quality: string|LuaQualityPrototype|nil, count: number})[]
---@param truncate integer?
---@param protos table<string, LuaItemPrototype | LuaFluidPrototype>?
---@param raw boolean? If true, return just the list without "Has" wrapper
---@return LocalisedString
function mod.present_list(list, truncate, protos, raw)
   local contents = TH.rollup2(list, F.name().get, function(i)
      return i.quality or "normal"
   end, F.count().get)

   -- Now that everything is together we must unroll it again, then sort.
   ---@type ({ count: number, name: string, quality: LuaQualityPrototype })[]
   local final = {}

   for name, quals in pairs(contents) do
      for qual, count in pairs(quals) do
         table.insert(final, { count = count, name = name, quality = prototypes.quality[qual] })
      end
   end

   -- Careful: this is actually a reverse sort.
   table.sort(final, function(a, b)
      if a.count == b.count and a.name == b.name then
         return a.quality.level > b.quality.level
      elseif a.count == b.count then
         return a.name > b.name
      else
         return a.count > b.count
      end
   end)

   local endpoint = #final
   local extra = false
   if truncate then
      extra = truncate < endpoint
      endpoint = math.min(endpoint, truncate)
   end

   if not next(final) then return { "fa.ent-info-inventory-empty" } end

   local entries = {}
   for i = 1, endpoint do
      local e = final[i]

      table.insert(
         entries,
         ItemInfo.item_or_fluid_info({ name = e.name, quality = e.quality, count = e.count }, protos or prototypes.item)
      )
   end

   if extra then
      table.insert(entries, ItemInfo.item_info({ name = ItemInfo.ITEM_OTHER, count = #final - truncate }))
   end
   local joined = FaUtils.localise_cat_table(entries, ", ")

   if raw then return joined end
   return { "fa.ent-info-inventory-presentation", joined }
end

return mod
