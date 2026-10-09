-- Setting declarations registered by settings.lua and read at runtime.
-- Players change them in the game's own Mod settings window.

---@class fa.SettingDecl
---@field name string
---@field type "bool-setting"|"int-setting"|"double-setting"|"string-setting"
---@field setting_type "startup"|"runtime-global"|"runtime-per-user"
---@field default_value boolean|number|string
---@field order string
---@field minimum_value number?
---@field maximum_value number?
---@field allowed_values string[]?

local mod = {}

mod.SETTING_NAMES = {
   SONIFICATION_INSERTER = "fa-inserter-sonification",
   SONIFICATION_CRAFTING = "fa-crafting-sonification",
   SONIFICATION_COMBAT_ENEMIES = "fa-combat-enemy-sonification",
   SONIFICATION_COMBAT_SPAWNERS = "fa-combat-spawner-sonification",
}

---@type fa.SettingDecl[]
mod.declarations = {
   {
      name = mod.SETTING_NAMES.SONIFICATION_INSERTER,
      type = "bool-setting",
      setting_type = "runtime-per-user",
      default_value = true,
      order = "a",
   },
   {
      name = mod.SETTING_NAMES.SONIFICATION_CRAFTING,
      type = "bool-setting",
      setting_type = "runtime-per-user",
      default_value = true,
      order = "b",
   },
   {
      name = mod.SETTING_NAMES.SONIFICATION_COMBAT_ENEMIES,
      type = "bool-setting",
      setting_type = "runtime-per-user",
      default_value = true,
      order = "c",
   },
   {
      name = mod.SETTING_NAMES.SONIFICATION_COMBAT_SPAWNERS,
      type = "bool-setting",
      setting_type = "runtime-per-user",
      default_value = true,
      order = "d",
   },
}

return mod
