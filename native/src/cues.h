#pragma once

#include "skiprules.h"

// Sounds the DLL plays itself: the next machine's sound, which the map cursor's step plays from Lua
// (control.lua's play_next_of_kind), on a step in a blueprint's picture where the mod's Lua never
// hears the key; and the cursor's tones on coming into line with the player's audio rulers, on
// frames, which go on while the map is paused. Only this client hears them; they change nothing in
// the game.
namespace fa::cues {

// From the mod's Lua, where it runs (fa_native.cursor_mode): whether this client's player has the
// next machine's sound on. The first call reads the sounds' files.
void setNextMachine(bool enabled);

// The next machine's sound, when the player has it on.
void nextMachine();

// The tone of the cursor coming onto a tile in line with the player's audio rulers, as
// scripts/rulers.lua plays the character's: a click on a ruler, a high tone on a line, a low one a
// tile off it.
void rulerTone(skip::Alignment alignment);

} // namespace fa::cues
