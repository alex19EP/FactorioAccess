#pragma once

// Sounds the DLL's own screens play where the mod's Lua never hears the key: the next machine's
// sound, which the map cursor's step plays from Lua (control.lua's play_next_of_kind), on a step
// in a blueprint's picture. Only this client hears them; they change nothing in the game.
namespace fa::cues {

// From the mod's Lua, where it runs (fa_native.cursor_mode): whether this client's player has the
// next machine's sound on. The first call reads the sound's file.
void setNextMachine(bool enabled);

// The next machine's sound, when the player has it on.
void nextMachine();

} // namespace fa::cues
