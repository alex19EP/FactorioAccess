#pragma once

#include <cstdint>
#include <string>

// What a pinned building or blueprint (cursor.h, detaching) would build, piece by piece, as the
// game's build preview draws and checks it: each piece's name, the way it faces, its tiles, and
// whether it can be built there, is already built, or what blocks it. The detached cursor reads the
// piece on its tile before the tile itself, so the player explores how the build fits without
// moving it.
//
// The preview checks only the pieces near the screen, as the game draws them; the cursor stays on
// the screen, so the pieces it reaches are always among them.
namespace fa::footprint {

// From world.cpp's preview hooks, on the thread that draws the preview: whether to gather the
// pieces of the build drawn at `x`, `y` (fixed point) this frame. True when it moved, now and then
// for a world that changes under it, and while an announcement waits.
bool wants(int32_t x, int32_t y);

enum class Fit { Fits, AlreadyBuilt, Blocked };
struct Piece {
   std::string name;
   int direction; // 16-way defines.direction; -1 for a piece without one
   int32_t left;  // tiles covered, first and last column and row
   int32_t top;
   int32_t right;
   int32_t bottom;
   Fit fit;
   std::string reason; // the game's, for a blocked piece
};
// Between begin and end, every piece the preview drew. A blueprint's pieces get a summary.
void begin(bool blueprint);
void add(Piece piece);
void end(int32_t x, int32_t y);

// Forgets the pieces: the build is no longer pinned.
void clear();

// For a step of the detached cursor onto the tile `tileX`, `tileY`: the piece there, or that the
// tile is a gap inside the build, or that the step left the build; empty elsewhere. Before the
// tile's own reading.
std::string tilePrefix(int32_t tileX, int32_t tileY);

// Says `lead`, then for a blueprint what blocks it or is already built, then the piece on the tile
// `tileX`, `tileY`, once the pieces of the build where it is now pinned are gathered: after a
// nudge, the frame after.
void announce(std::string lead, int32_t tileX, int32_t tileY);

} // namespace fa::footprint
