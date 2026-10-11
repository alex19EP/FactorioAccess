-- The audio ruler tool and its shortcut bar button (scripts/rulers.lua). As Kruise Kontrol's
-- spaghetti planner, the tool is a selection tool only so that holding it gives the cursor to the
-- ruler: a click neither builds nor opens a window. Clicks come from fa-audio-ruler-place and
-- fa-audio-ruler-remove (data/input.lua), never from a selected area.

local invisible = { 0, 0, 0, 0 }

data:extend({
   {
      type = "selection-tool",
      name = "fa-audio-ruler",
      icon = "__FactorioAccess__/Graphics/audio-ruler.png",
      icon_size = 64,
      flags = { "only-in-cursor", "not-stackable", "spawnable" },
      auto_recycle = false,
      hidden = true,
      subgroup = "tool",
      order = "z[audio-ruler]",
      stack_size = 1,
      select = {
         border_color = invisible,
         mode = { "nothing" },
         cursor_box_type = "copy",
      },
      alt_select = {
         border_color = invisible,
         mode = { "nothing" },
         cursor_box_type = "copy",
      },
   },

   {
      type = "shortcut",
      name = "fa-audio-ruler",
      order = "z[audio-ruler]",
      action = "lua",
      associated_control_input = "fa-ca-b",
      icon = "__FactorioAccess__/Graphics/audio-ruler-x56.png",
      icon_size = 56,
      small_icon = "__FactorioAccess__/Graphics/audio-ruler-x24.png",
      small_icon_size = 24,
   },
})
