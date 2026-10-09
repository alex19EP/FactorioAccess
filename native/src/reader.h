#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// The tile reader: what a movement key's step finds where the cursor lands, said by the DLL as the
// key is pressed, without waiting for the mod's Lua to hear of the key from the server.
//
// It says what a sighted player sees on hovering there. The entity is the one the game selects
// under the mouse (EntitySelector::deduceSelectedEntity, fog of war included), and what is said of
// it comes from the game's own tooltip (the entity's addToDescription), less what the tooltip shows
// alike for every entity of the kind (the mod it comes from, its description, wire reach, maximum
// consumption, the last player ...): its name, a status only when it tells of a problem, health
// only when damaged, then the lines that tell of this entity's state. With no entity there, the
// tile's name. What the game cannot say (a belt's shape, a pipe's connections, water shores), the
// mod's Lua says after it, through `more`.
namespace fa::reader {

// The cursor took a movement key to the tile `tileX`, `tileY`. The next tick of the application Gui
// reads it. On the main thread.
void moved(int32_t tileX, int32_t tileY);

// In the application Gui's logic: reads a pending step.
void tick();

// The mod's own words for the tile `tileX`, `tileY`, said after the reading when it was the last
// one read (a later step has made them stale). Safe from any thread.
void more(int32_t tileX, int32_t tileY, std::string_view text);

// The entity the reading of the tile `tileX`, `tileY` described, for the mod's own words about it:
// its prototype's name and its position in tiles. Nothing for a tile read without one, or for
// another tile. Safe from any thread.
struct Entity {
   std::string name;
   double x;
   double y;
};
std::optional<Entity> entity(int32_t tileX, int32_t tileY);

} // namespace fa::reader
