#pragma once

#include <optional>

// The FA cursor: the tile the blind player points at, which this client alone owns. The game reads
// it wherever it reads the mouse (world.h), so hover selection, building and every mod key's
// event (CustomInputEvent::cursor_position) act there, and the server and the other clients learn
// it from those inputs. The mod's Lua keeps no cursor of its own.
//
// The movement keys (fa-w, fa-a, fa-s, fa-d) move it a tile, or on the full map a cell, as the
// game reads the key: the key's own event already carries the new position, with no wait for the
// server. It reaches only what a sighted player's mouse reaches: in the character and god views,
// whose camera stays on the player, the tiles on the screen, and the movement keys stop at the
// edge. In remote view the camera follows the cursor, moved as a sighted player's map drag moves
// it, and a camera the game moves (an alert, a pin, a search result) brings the cursor along.
namespace fa::cursor {

// From the mod's Lua every tick, for this client's player: whether the mod is in vanilla mode (the
// mouse is the cursor), whether the movement keys move the cursor (no mod menu has them and combat
// mode does not aim with them), and whether the cursor is anchored to the character (it goes back
// to the character, not to the screen's edge, when the screen leaves it behind).
void setMode(int playerIndex, bool vanilla, bool keys, bool anchored);

// The mod moves the cursor to the tile holding (x, y): its own jumps, such as to an alert or a
// scanner entry. A tile off the screen in the character or god view opens remote view there.
void jump(int playerIndex, double x, double y);

// What a movement key did, for the key's event that carries (x, y): moved there and the tile reader
// read it (reader.h), moved there unread (a cell of the full map, a step of a drag build), or stayed
// at the edge of the screen. Nothing when the cursor did not take the key. Each result is given once.
enum class Step { Read, Moved, Edge };
std::optional<Step> step(int playerIndex, double x, double y);

// The cursor's tile, for this client's player. Only this client knows it: the mod may speak and
// play sounds by it, never change the game by it.
struct Tile {
   int x;
   int y;
};
std::optional<Tile> position(int playerIndex);

// MinHook detours for PlayerInputSource::processCustomInput and
// PlayerInputSource::sendRenderStateChanges, and where MinHook keeps the originals.
void* customInputDetour();
void** customInputOriginal();
void* renderStateDetour();
void** renderStateOriginal();

} // namespace fa::cursor
