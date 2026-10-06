#pragma once

// The game world under the FA cursor. The mod reports where its cursor is, and the game's own
// cursor position follows it: hover selection, building, mining, opening entities and every
// selection tool act there as they would at the mouse. The keys that do so are the game's own
// controls; helper-scripts/bind-mouse-keys.ps1 gives every mouse button binding a key.
namespace fa::world {

// From the mod's Lua, on whatever thread runs it. `playerIndex` is LuaPlayer::index; only the
// local player's calls count. Positions are map positions in tiles.
void setCursor(int playerIndex, double x, double y);
// Gives the game cursor back to the mouse.
void releaseCursor(int playerIndex);
// Whether `playerIndex` may be this client's player: false only once a game with a local player
// is up and that player is someone else.
bool mayBeLocalPlayer(int playerIndex);

// MinHook detours for Player::getCursorMapPosition and PlayerInputSource::getCursorMapPosition,
// and where MinHook keeps the originals.
void* playerCursorDetour();
void** playerCursorOriginal();
void* sourceCursorDetour();
void** sourceCursorOriginal();

} // namespace fa::world
