--[[
Time of day. The game shows no clock, only how light it is, so this speaks the part of the day and,
while the light changes, how much of it there is. Each surface has its own cycle: dusk, evening,
morning and dawn are points in its daytime, which runs from noon (0) through midnight (0.5). A surface
with always_day is held at noon.
]]
local mod = {}

---@param surface LuaSurface
---@return LocalisedString
function mod.describe(surface)
   local time = surface.daytime
   local light = math.floor((1 - surface.darkness) * 100 + 0.5)
   if time >= surface.dusk and time < surface.evening then
      return { "fa.daytime-dusk", light }
   elseif time >= surface.evening and time < surface.morning then
      return { "fa.daytime-night" }
   elseif time >= surface.morning and time < surface.dawn then
      return { "fa.daytime-dawn", light }
   end
   return { "fa.daytime-day" }
end

return mod
