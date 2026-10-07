#pragma once

#include <cstdint>

// How this client's character's walking went, as the game decides it: every tick the character
// walks, Character::changePosition moves it the whole way, slides it along what it ran into, or
// leaves it where it was. The mod's bump sounds follow this rather than guessing from positions.
namespace fa::movement {

enum class Step { Full = 0, Partial = 1, None = 2 };

// The step `playerIndex`'s (LuaPlayer::index) character took this tick or the one before, or false
// when it took none, or that is not this client's player. `count` goes up by one with every step,
// wrapping at 256, so a step read twice can be told from the next. Only this client knows it, so
// the mod may play sounds by it but must not change the game by it.
bool recentStep(int playerIndex, Step& out, uint8_t& count);

// MinHook detour for Character::changePosition, and where MinHook keeps the original.
void* changePositionDetour();
void** changePositionOriginal();

} // namespace fa::movement
