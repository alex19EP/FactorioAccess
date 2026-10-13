--Here: mining functions
local mod = {}

function mod.play_mining_sound(pindex)
   local player = game.players[pindex]
   if player and player.mining_state.mining and player.selected and player.selected.valid then
      if player.selected and player.selected.prototype.is_building then
         player.play_sound({ path = "player-mine" })
      elseif player.selected and player.selected.type ~= "resource" then
         player.play_sound({ path = "player-mine" }) --Mine other things, eg. character corpses, laterdo new sound
      end
   end
end

return mod
