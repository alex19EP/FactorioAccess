--[[
Announces finished research to every player of the force, with the recipes it unlocks.

The technology screen itself is the game's own window (T), read by the native DLL.
]]
local FaUtils = require("scripts.fa-utils")
local Localising = require("scripts.localising")
local Speech = require("scripts.speech")
local TH = require("scripts.table-helpers")

local mod = {}

---@param event EventData.on_research_finished
function mod.on_research_finished(event)
   local tech = event.research
   local name_str = Localising.get_localised_name_with_fallback(tech)
   local recipes = {}

   for _, v in pairs(tech.prototype.effects) do
      if v.type == "unlock-recipe" then table.insert(recipes, v.recipe) end
   end

   local announcing = { "fa.research-finished-plain", name_str }
   if next(recipes) then
      local namified = TH.map(recipes, function(r)
         return Localising.get_localised_name_with_fallback(prototypes.recipe[r])
      end)

      local joined = FaUtils.localise_cat_table(namified, ", ")
      announcing = { "fa.research-finished-with-recipes", name_str, joined }
   end

   local force = tech.force
   local players = force.players
   for _, p in pairs(players) do
      Speech.speak(p.index, announcing)
   end
end

return mod
