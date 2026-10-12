-- Until 2026-10-07 the mod turned mouse-over selection off every tick, and saves keep it so. The
-- game's cursor now follows the FA cursor, so the game selects what the cursor is over, as its
-- default does. Kruise Kontrol turns it off while it mines and back on after.
for _, player in pairs(game.players) do
   player.game_view_settings.update_entity_selection = true
end
