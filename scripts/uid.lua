--[[
Unique ids

Sometimes it is very useful to generate ids which are unique per save.  This is
used for example in movement history.

At first it may seem that one might simply use tables.  This works for
everything but deletion.  Usually a table is one level too deep for removal.  By
using an integral id, removal itself becomes easy.

]]

local mod = {}

---@return number
function mod.uid()
   if not storage.id_counter then storage.id_counter = 0 end
   storage.id_counter = storage.id_counter + 1
   return storage.id_counter
end

return mod
