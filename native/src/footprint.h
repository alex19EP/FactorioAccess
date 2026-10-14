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

// A blueprint in hand that follows the cursor is gathered too, for its summary alone: what blocks it
// and what is already built is said each time it moves or turns, as the preview of a building in
// hand says what blocks it.
//
// Where the build is drawn (fixed point), how it is turned and flipped, and whether it is pinned.
struct Placement {
   int32_t x;
   int32_t y;
   uint32_t turn;
   bool pinned;
   bool operator==(const Placement&) const = default;
};

// From world.cpp's preview hooks, on the thread that draws the preview: whether to gather the
// pieces of the build drawn at `placement` this frame. True when it moved or turned; for a pinned
// build also now and then, for a world that changes under it, and while an announcement waits.
bool wants(const Placement& placement);

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
// Between begin and end, on the thread that called begin, every piece the preview drew (a
// blueprint's tiles among them) and each entity on the map that building it would replace, turn,
// remove or otherwise change, with the words that say so ("removes Tree") and its tiles. A
// blueprint's pieces get a summary. While the build control drags, a blueprint that follows the
// cursor has just been built where it stands, and its summary leaves out what is already built.
void begin(const Placement& placement, bool blueprint, bool drag);
bool gathering();
void add(Piece piece);
void alter(std::string words, int32_t left, int32_t top, int32_t right, int32_t bottom);
void end();

// Forgets the pieces: the build is no longer pinned.
void clear();

// For a step of the detached cursor onto the tile `tileX`, `tileY`: the pieces there and what
// building them does to the map there, or that the step left the build; empty elsewhere, and while
// the build follows the cursor. Before the tile's own reading.
std::string tilePrefix(int32_t tileX, int32_t tileY);

// Says `lead`, then for a blueprint what blocks it or is already built, then what is on the tile
// `tileX`, `tileY` as a step reads it, once the pieces of the build where it is now pinned are gathered: after a
// nudge, the frame after.
void announce(std::string lead, int32_t tileX, int32_t tileY);

} // namespace fa::footprint
