#pragma once

#include <optional>
#include <string>

// The FA cursor: the tile the blind player points at, which this client alone owns. The game reads
// it wherever it reads the mouse (world.h), so hover selection, building and every mod key's
// event (CustomInputEvent::cursor_position) act there, and the server and the other clients learn
// it from those inputs. The mod's Lua keeps no cursor of its own.
//
// The movement keys (fa-w, fa-a, fa-s, fa-d) move it a tile, or on the full map a cell, as the
// game reads the key: the key's own event already carries the new position, with no wait for the
// server. The skip keys (fa-s-w ...) move it to the next tile that differs (skip.h), and Ctrl with
// a movement key (fa-c-w ...) by the size of the building or blueprint in hand, the same way. The
// cycle key (fa-s-f) moves it within its tile to the next point where the mouse selects something
// else, so the game selects each overlapping thing in turn; a movement key goes back to the centre.
//
// It reaches only what a sighted player's mouse reaches: in the character and god views, whose
// camera stays on the player, the tiles on the screen, and the movement keys stop at the edge. In
// remote view the camera follows the cursor, moved as a sighted player's map drag moves it, and a
// camera the game moves (an alert, a pin, a search result) brings the cursor along.
namespace fa::cursor {

// From the mod's Lua every tick, for this client's player: whether the mod is in vanilla mode (the
// mouse is the cursor), whether the movement and skip keys move the cursor (no mod menu has them
// and combat mode does not aim with them), whether the cursor is anchored to the character (it goes
// back to the character, not to the screen's edge, when the screen leaves it behind), and the tile
// of the player's audio ruler, where skips stop (skip.h).
struct Tile {
   int x;
   int y;
};
void setMode(int playerIndex, bool vanilla, bool keys, bool anchored, std::optional<Tile> ruler);

// The mod moves the cursor to the tile holding (x, y): its own jumps, such as to an alert or a
// scanner entry. A tile off the screen in the character or god view opens remote view there.
void jump(int playerIndex, double x, double y);

// What a movement or skip key did, for the key's event that carries (x, y): moved there and the
// tile reader read it (reader.h), moved there unread (a cell of the full map, a step of a drag
// build), stayed at the edge of the screen, or skipped there (Shift+WASD) over `tiles` tiles, or
// found no change within the skip's limit (-1), and the reader read it. Ctrl+WASD, which moves by
// the size of the building in hand, reads as a step. Nothing when the cursor did not take the key.
// Each result is given once.
enum class Step { Read, Moved, Edge, Skipped };
struct StepResult {
   Step step;
   int tiles;
};
std::optional<StepResult> step(int playerIndex, double x, double y);

// The cursor's tile, for this client's player. Only this client knows it: the mod may speak and
// play sounds by it, never change the game by it.
std::optional<Tile> position(int playerIndex);

// For the dev server's /cursor, on the main thread: this client's view (its controller, the
// camera's centre and zoom, the tiles on the screen, the full map's cell size) and the cursor's
// tile, a `name=value` a line.
std::string describe();

// MinHook detours for PlayerInputSource::processCustomInput and
// PlayerInputSource::sendRenderStateChanges, and where MinHook keeps the originals.
void* customInputDetour();
void** customInputOriginal();
void* renderStateDetour();
void** renderStateOriginal();

} // namespace fa::cursor
