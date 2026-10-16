--[[
Some common table helping algorithms.

there's a number of table things like removing invalid entities which we need
everywhere efficienhtly; this file does that.
]]

local mod = {}

-- Merges two arrays.  The second array is pushed into the first.  That is, it
-- is *modified* in place.
--
---@param destination any[]
---@param array any[]
function mod.concat_arrays(destination, array)
   -- faster: table.insert is a hashtable lookup.
   local tins = table.insert

   for i = 1, #array do
      tins(destination, array[i])
   end
end

local empty_table_defaulter = {
   __index = function(t, i)
      rawset(t, i, {})
      return t[i]
   end,
}
-- ds/ requires this module under plain Lua, where script does not exist.
---@diagnostic disable-next-line: unnecessary-if
if script then script.register_metatable("fa.TableHelpers.EmptyTableDefaulter", empty_table_defaulter) end

--[[
Returnn an empty table.  When an index not yet present in the table is accessed,
fill it in with an empty table as well.  If the optional argument initial is
provided, wrap that instead and return it.

This means code like `a[5][4]` does not need to check if 5 is present.  This is
very useful for making sets of 2d points and objects, but the cost is that a
check like `if set[x][y]` will fill in x with an empty table even if the item is
not present (but that's usually fine, because the most common operation is to
then add it).

IMPORTANT: the obvious extension is to allow changing ther default value.  That
doesn't work because storage cannot hold unregistered metatables.  There'd need
to be a unique name each.  Other methods result in losing the benefit or are
much more complex and should be avoided.
]]
function mod.defaulting_table(initial)
   local r = initial or {}
   assert(type(r) == "table")
   setmetatable(r, empty_table_defaulter)
   return r
end

-- Return a copy of table, with func called on all values.
---@param tab table
---@param func fun(any): any
---@return table
function mod.map(tab, func)
   local copy = {}
   for k, v in pairs(tab) do
      copy[k] = func(v)
   end
   return copy
end

--[[
Take an array, 3 functions which unpack it.  Unroll that into a table of
func1->func2->count, where func1 and func2 are the results of the first two
functions.  To make this convenient, consider field-ref.

This is useful because it can take (for example) a transport line's detailed
contents, which has quality and prototypes, and roll that up into a
prototype->quality->count table.
]]
---@param input any[]
---@param func1 fun(any): any
---@param func2 fun(any): any
---@param func3 fun(any): number
---@return table<any, table<any, number>>
function mod.rollup2(input, func1, func2, func3)
   local counts = mod.defaulting_table()

   for i = 1, #input do
      local item = input[i]
      local k1 = func1(item)
      local k2 = func2(item)
      local c = func3(item)
      local c2 = counts[k1]
      c2[k2] = (c2[k2] or 0) + c
   end

   setmetatable(counts, nil)
   return counts
end

--[[
When doing announcements we usually need to go from NQC to sorted data in
descending order. This does that, in a form that can be directly passed to
localisation.
]]
---@param tab fa.NQC
---@return ({ name: string, quality: string, count: number})[]
function mod.nqc_to_sorted_descending(tab)
   local ret = {}

   for n, quals in pairs(tab) do
      for qual, c in pairs(quals) do
         table.insert(ret, { name = n, quality = qual, count = c })
      end
   end

   table.sort(ret, function(a, b)
      return a.count > b.count
   end)
   return ret
end

--[[
Returns a table with a metatable that prevents all reads, writes and pairs() loops. This is useful for catching
accidental usage of deprecated global variables. Any attempt throws an error: a loop over the empty table would
otherwise run nothing, silently.
]]
---@return table
function mod.deny_access_table()
   local denied = {}
   local mt = {
      __index = function(t, k)
         error(
            "Attempted to read from denied table at key: " .. tostring(k) .. "\nUse storage.players instead of players"
         )
      end,
      __newindex = function(t, k, v)
         error(
            "Attempted to write to denied table at key: " .. tostring(k) .. "\nUse storage.players instead of players"
         )
      end,
      __pairs = function()
         error("Attempted to iterate a denied table\nUse storage.players or game.connected_players instead of players")
      end,
   }
   setmetatable(denied, mt)
   return denied
end

return mod
