-- data/input.lua
-- Defines custom-input prototypes for FactorioAccess.

data:extend({
   {
      type = "custom-input",
      name = "fa-w",
      key_sequence = "W",
      consuming = "none",
   },
   {
      type = "custom-input",
      name = "fa-a",
      key_sequence = "A",
      consuming = "none",
   },
   {
      type = "custom-input",
      name = "fa-s",
      key_sequence = "S",
      consuming = "none",
   },
   {
      type = "custom-input",
      name = "fa-d",
      key_sequence = "D",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-s-w",
      key_sequence = "SHIFT + W",
      consuming = "none",
   },
   {
      type = "custom-input",
      name = "fa-s-a",
      key_sequence = "SHIFT + A",
      consuming = "none",
   },
   {
      type = "custom-input",
      name = "fa-s-s",
      key_sequence = "SHIFT + S",
      consuming = "none",
   },
   {
      type = "custom-input",
      name = "fa-s-d",
      key_sequence = "SHIFT + D",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-c-w",
      key_sequence = "CONTROL + W",
      consuming = "none",
   },
   {
      type = "custom-input",
      name = "fa-c-a",
      key_sequence = "CONTROL + A",
      consuming = "none",
   },
   {
      type = "custom-input",
      name = "fa-c-s",
      key_sequence = "CONTROL + S",
      consuming = "none",
   },
   {
      type = "custom-input",
      name = "fa-c-d",
      key_sequence = "CONTROL + D",
      consuming = "none",
   },
   {
      type = "custom-input",
      name = "fa-t",
      key_sequence = "T",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-k",
      key_sequence = "K",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-s-k",
      key_sequence = "SHIFT + K",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-a-k",
      key_sequence = "ALT + K",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-j",
      key_sequence = "J",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-a-j",
      key_sequence = "ALT + J",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-ca-b",
      key_sequence = "CONTROL + ALT + B",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-i",
      key_sequence = "I",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-pageup",
      key_sequence = "PAGEUP",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-s-pageup",
      key_sequence = "SHIFT + PAGEUP",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-c-pageup",
      key_sequence = "CONTROL + PAGEUP",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-pagedown",
      key_sequence = "PAGEDOWN",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-s-pagedown",
      key_sequence = "SHIFT + PAGEDOWN",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-c-pagedown",
      key_sequence = "CONTROL + PAGEDOWN",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-end",
      key_sequence = "END",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-home",
      key_sequence = "HOME",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-s-end",
      key_sequence = "SHIFT + END",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-c-tab",
      key_sequence = "CONTROL + TAB",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-cs-tab",
      key_sequence = "CONTROL + SHIFT + TAB",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-f",
      key_sequence = "",
      linked_game_control = "pick-items",
      consuming = "none",
   },

   -- The DLL's cursor takes it (native/src/cursor.h); the mod's Lua has no handler
   {
      type = "custom-input",
      name = "fa-s-f",
      key_sequence = "SHIFT + F",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-delete",
      key_sequence = "DELETE",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-s-x",
      key_sequence = "SHIFT + X",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-cs-x",
      key_sequence = "CONTROL + SHIFT + X",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-r",
      key_sequence = "",
      linked_game_control = "rotate",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-s-r",
      key_sequence = "",
      linked_game_control = "reverse-rotate",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-h",
      key_sequence = "",
      linked_game_control = "flip-horizontal",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-v",
      key_sequence = "",
      linked_game_control = "flip-vertical",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-y",
      key_sequence = "Y",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-s-y",
      key_sequence = "SHIFT + Y",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-cas-v",
      key_sequence = "CONTROL + ALT + SHIFT + V",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-s-q",
      key_sequence = "SHIFT + Q",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-s-v",
      key_sequence = "SHIFT + V",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-c-r",
      key_sequence = "CONTROL + R",
      consuming = "none",
   },

   {
      type = "custom-input",
      name = "fa-cs-r",
      key_sequence = "CONTROL + SHIFT + R",
      consuming = "none",
   },

   -- A click and a Shift+click with the audio ruler in hand, on whatever keys the player gave the
   -- game's select and alternative select.
   {
      name = "fa-audio-ruler-place",
      type = "custom-input",
      linked_game_control = "select-for-blueprint",
      consuming = "none",
      key_sequence = "",
   },

   {
      name = "fa-audio-ruler-remove",
      type = "custom-input",
      linked_game_control = "select-for-cancel-deconstruct",
      consuming = "none",
      key_sequence = "",
   },

   {
      type = "custom-input",
      name = "fa-cas-i",
      key_sequence = "CONTROL + ALT + SHIFT + I",
      consuming = "game-only",
   },

   {
      type = "custom-input",
      name = "fa-cas-c",
      key_sequence = "CONTROL + ALT + SHIFT + C",
      consuming = "game-only",
   },

   {
      type = "custom-input",
      name = "fa-cas-e",
      key_sequence = "CONTROL + ALT + SHIFT + E",
      consuming = "game-only",
   },

   {
      type = "custom-input",
      name = "fa-cas-s",
      key_sequence = "CONTROL + ALT + SHIFT + S",
      consuming = "game-only",
   },

   {
      type = "custom-input",
      name = "fa-cs-i",
      key_sequence = "CONTROL + SHIFT + I",
      consuming = "none",
   },

   -- Combat mode capsule force-fire (ctrl+shift+wasd)
   {
      type = "custom-input",
      name = "fa-cs-w",
      key_sequence = "CONTROL + SHIFT + W",
      consuming = "none",
   },
   {
      type = "custom-input",
      name = "fa-cs-a",
      key_sequence = "CONTROL + SHIFT + A",
      consuming = "none",
   },
   {
      type = "custom-input",
      name = "fa-cs-s",
      key_sequence = "CONTROL + SHIFT + S",
      consuming = "none",
   },
   {
      type = "custom-input",
      name = "fa-cs-d",
      key_sequence = "CONTROL + SHIFT + D",
      consuming = "none",
   },
})
