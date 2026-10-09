---Make the character unlikely to be selected by the mouse pointer when overlapping with entities
data.raw.character.character.selection_priority = 2

-- Modifications to Kruise Kontrol inputs (no longer needed)
-- We will handle Kruise Kontrol driving through the remote API.  It binds
-- everything to the mouse, which we don't use.  The exception is enter, which
-- cancels.  We also cancel on enter, but double-cancel doesn't do anything.
-- This file used to modify those inputs, but we don't need to since things
-- already work.  If we do need to revisit that, note that we will need to move
-- KK inputs to a dummy key, or alternatively try setting [alt]_key_sequence to
-- the empty string.  Other solutions (e.g. removal, setting them to disabled)
-- break KK because Factorio will not let KK register events.

--Modifications to Pavement Driving Assist Continued inputs
data:extend({
   {
      type = "custom-input",
      name = "toggle_drive_assistant",
      key_sequence = "L",
      consuming = "game-only",
   },
   {
      type = "custom-input",
      name = "toggle_cruise_control",
      key_sequence = "O",
      consuming = "game-only",
   },
   {
      type = "custom-input",
      name = "set_cruise_control_limit",
      key_sequence = "CONTROL + O",
      consuming = "game-only",
   },
   {
      type = "custom-input",
      name = "confirm_set_cruise_control_limit",
      key_sequence = "",
      linked_game_control = "confirm-gui",
   },
})
