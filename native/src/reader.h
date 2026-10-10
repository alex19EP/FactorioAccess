#pragma once

#include <cstdint>
#include <string>

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
// the icons alt mode draws. With no entity there, the tile's name and a water tile's shape. Last,
// whether the player's force has not charted the tile, or charted it but does not see it now.
namespace fa::reader {

// Reads the tile `tileX`, `tileY` at the application Gui's next tick, after `prefix` when it is not
// empty (a skip's distance, the mod's words for its jump). A later call before that tick replaces
// it. Safe from any thread.
void read(int32_t tileX, int32_t tileY, std::string prefix = {});

// In the application Gui's logic: reads a pending tile.
void tick();

// For the skip keys (skip.h), in the game's input handling, where the game itself finds what the
// mouse selects: what the mouse at the centre of the tile `tileX`, `tileY` would select for this
// client's player (null for nothing, or outside a game with a view), and whether the tile is water.
const void* selectedAt(int32_t tileX, int32_t tileY);
bool waterAt(int32_t tileX, int32_t tileY);

} // namespace fa::reader
