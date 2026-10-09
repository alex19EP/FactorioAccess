--Here: recipe lists for crafting machines

local mod = {}

--Returns a navigable list of all unlocked recipes, for the recipe categories supported by the selected entity. Optionally can return all unlocked recipes for all categories.
function mod.get_recipes(pindex, ent, load_all_categories)
   if not ent then return {} end
   local category_filters = {}
   --Load the supported recipe categories for this entity
   for category_name, _ in pairs(ent.prototype.crafting_categories) do
      table.insert(category_filters, { filter = "category", category = category_name })
   end
   local all_machine_recipes = prototypes.get_recipe_filtered(category_filters)
   local unlocked_machine_recipes = {}
   local force_recipes = game.get_player(pindex).force.recipes

   --Load all crafting categories if instructed
   if load_all_categories == true then
      ---@diagnostic disable-next-line: cast-local-type
      all_machine_recipes = force_recipes
   end

   --Load only the unlocked recipes
   for recipe_name, recipe in pairs(all_machine_recipes) do
      local force_recipe = force_recipes[recipe_name]
      local proto = prototypes.recipe[recipe_name]
      -- only include enabled, non-hidden recipes that actually produce items
      if force_recipe and force_recipe.enabled and not force_recipe.hidden and proto and next(proto.products) then
         local grp = recipe.group.name
         unlocked_machine_recipes[grp] = unlocked_machine_recipes[grp] or {}
         table.insert(unlocked_machine_recipes[grp], force_recipe)
      end
   end
   local result = {}
   for group, recipes in pairs(unlocked_machine_recipes) do
      table.insert(result, recipes)
   end
   return result
end

return mod
