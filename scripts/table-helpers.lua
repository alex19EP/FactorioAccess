--[[
Some common table helping algorithms.

there's a number of table things like removing invalid entities which we need
everywhere efficienhtly; this file does that.
]]

local mod = {}

-- Same as table.insert(x), except taking multiple arguments and pushing them to
-- the back left to right. Behavior is incredibly undefined is nil is passed
-- (LuaLS helps guard against it)
--
---@param destination any[]
---@param ... any
function mod.multipush(destination, ...)
   local packed = table.pack(...)
   mod.concat_arrays(destination, packed)
end

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

-- Takes an array { a, b, c } and writes to to a set { a = true, b = true, c =
-- true }.
---@param set table<any, true>
---@param array any[]
---@return table<any, true> A copy of set.
function mod.array_to_set(set, array)
   for i = 1, #array do
      set[array[i]] = true
   end
   return set
end

-- Merge the second mapping into the first (e.g. table of non-array keys)
---@param dest table<any, any>
---@param src table<any, any>
function mod.merge_mappings(dest, src)
   for k, v in pairs(src) do
      dest[k] = v
   end
end

local empty_table_defaulter = {
   __index = function(t, i)
      rawset(t, i, {})
      return t[i]
   end,
}
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

--[[
Return a metatable which will, when an index is not found, iterate over all of
the tables specified, left to right, before giving up.

There is a particularly useful trick which allows us to provide options to
functions which aren't safe for storage, usually callbacks.  To do it, we make
the outermost table storage-safe and store that.  Then, we hide the
non-storage-safe things away in tables which are consulted by the metatable,
since that never "pulls values up".  This comes with a negligible performance
hit, but it's usually only a couple levels and for a function, which means in
context that's not too bad (plus, anything truly performance sensitive will
cache in a local anyway).

Also, this is simpler Lua inheritance: list the most derived class first.

At least one table must be specified.
]]
function mod.nested_indexer(...)
   local args = table.pack(...)
   assert(#args > 0, "At least one table must be specified")
   local cache = {}

   return {
      __index = function(tab, key)
         local c = cache[key]
         if c then return c end

         for i = 1, #args do
            local attempt = args[i][key]
            if attempt then
               cache[key] = attempt
               return attempt
            end
         end

         return nil
      end,
   }
end

-- Find the index of a given element in a list. Return nil for not found.
function mod.find_index_of(array, element)
   for i = 1, #array do
      if array[i] == element then return i end
   end

   -- Not found.
   return nil
end

-- Convert a key-value table to an array of 2-tuples (k, v) then sort that by
-- k.
function mod.set_to_sorted_array(set)
   local array = {}
   for k, v in pairs(set) do
      table.insert(array, { k, v })
   end
   table.sort(array, function(a, b)
      return a[1] < b[1]
   end)
   return array
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

-- Given a two-level nested table, count the keys in both. Useful on e.g.
-- prototype->quality->count.  Is *not* a sum.
---@param table table<any, table<any, any>>
function mod.length2(table)
   local res = 0

   for _, v in pairs(table) do
      res = res + table_size(v)
   end

   return res
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

---@param tab table
---@return table
function mod.shallow_copy(tab)
   local out = {}
   for k, v in pairs(tab) do
      out[k] = v
   end
   return out
end

--[[
Returns a table with a metatable that prevents all reads and writes. This is useful for catching accidental usage of
deprecated global variables. Any attempt to read or write will throw an error.
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
   }
   setmetatable(denied, mt)
   return denied
end

return mod
