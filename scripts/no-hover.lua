local mod = {}

-- The game's cursor follows the FA cursor, so vanilla selects what it is over. Saves from before
-- the DLL turned this off. Every peer writes the same value, whether or not it runs the DLL.
function mod.on_tick()
   for _, p in pairs(game.players) do
      p.game_view_settings.update_entity_selection = true
   end
end

return mod
