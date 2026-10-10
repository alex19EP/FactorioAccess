#pragma once

#include <cstdint>
#include <string>
#include <vector>

// The tile reader: what is on the cursor's tile, said by the DLL. A movement or skip key's step is
// read as the key is pressed, without waiting for the mod's Lua to hear of the key from the server;
// the mod's own jumps are read when its Lua asks.
//
// It says what a sighted player sees on hovering there. The entity is the one the game selects
// under the mouse (EntitySelector::deduceSelectedEntity, fog of war included), and what is said of
// it comes from the game's own tooltip (the entity's addToDescription), less what the tooltip shows
// alike for every entity of the kind (the mod it comes from, its description, wire reach, maximum
// consumption, the last player ...): its name, FA's own facts (facts.h), a status only when it
// tells of a problem, health only when damaged, then the lines that tell of this entity's state and
// the icons alt mode draws. With no entity there, the tile's name and a water tile's shape. Where
// the view draws fog of war (remote view and the map), a tile the force has not charted is only
// "uncharted", and one it charted but does not see now is what the chart's old picture shows there
// (mappicture.h), then "blurred".
namespace fa::reader {

// Reads the tile `tileX`, `tileY` at the application Gui's next tick, after `prefix` when it is not
// empty (a skip's distance, the mod's words for its jump). A later call before that tick replaces
// it. Safe from any thread.
void read(int32_t tileX, int32_t tileY, std::string prefix = {});

// Reads what the mouse at the map position `x`, `y` (the game's fixed point, 256 to a tile) would
// select, or its tile, then `suffix`: the cycle key's stops inside a tile (cursor.h).
void readPoint(int32_t x, int32_t y, std::string suffix);

// In the application Gui's logic: reads a pending tile.
void tick();

// For the skip and cycle keys (skip.h, cursor.h), in the game's input handling, where the game
// itself finds what the mouse selects: what the mouse at the centre of the tile `tileX`, `tileY`,
// or at the map position `x`, `y` in fixed point, would select for this client's player (null for
// nothing, or outside a game with a view), and whether the tile is water.
const void* selectedAt(int32_t tileX, int32_t tileY);
const void* selectedAtPoint(int32_t x, int32_t y);
bool waterAt(int32_t tileX, int32_t tileY);

// The cycle key's stops in the tile `tileX`, `tileY` (cursor.h), in the game's input handling:
// each entity the mouse can select somewhere in the tile, with a point (fixed point) where it
// does, nearest the tile's centre; first what the centre selects, the rest in the game's own order
// of what takes the mouse (EntitySelector::selectPriorityLess, from the centre); last the bare
// ground (a null entity), where some point selects nothing. Empty outside a game with a view.
struct Stop {
   const void* entity;
   int32_t x;
   int32_t y;
};
std::vector<Stop> stopsIn(int32_t tileX, int32_t tileY);

} // namespace fa::reader
