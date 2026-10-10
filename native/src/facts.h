#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// FA's own facts about an entity, which the game's tooltip lacks: which way it faces, a belt's
// shape and contents, an underground belt's partner, a pipe's or heat pipe's shape, the fluid and
// heat connections on the tile, what a pole powers, an inserter's hand, the circuit networks and the
// like. The tile reader says them after the entity's name, in the mod's own words (the keys the
// mod's Lua said them with), each part a list item.
namespace fa::facts {

// The facts of `entity` as read on the tile `tileX`, `tileY`. In the application Gui's logic,
// where the game translates.
std::vector<std::string> describe(const void* entity, int32_t tileX, int32_t tileY);

// What the info icons the game draws on `entity` show (alt mode), in FA's words where it has them
// (fa-info.lua's ent_info_drawn_icons): the recipe as "producing", filters as "Filters for" or
// "Denies", fluids as "Has", a beacon's modules with counts, any other icon by name. Each part a
// list item; none while the game draws no icons. In the application Gui's logic.
std::vector<std::string> drawnIcons(const void* entity);

// What the skip keys tell entities apart by (control.lua's cursor_skip_iteration): its prototype (a
// ghost's building's), the way it faces when it has a direction, and for a transport belt how many
// belts feed it and it feeds and its corner. A skip goes on over the same entity and over equals.
struct Likeness {
   const void* prototype = nullptr;
   bool ghost = false;
   int direction = -1;
   size_t beltInputs = 0;
   size_t beltOutputs = 0;
   uint32_t beltShape = ~0u;
   bool operator==(const Likeness&) const = default;
};
Likeness likenessOf(const void* entity);

// Whether the skip keys pass over `entity` as if it were not there: robots and corpses, which lie
// on or fly over what is built. A character's corpse stays.
bool skipsOver(const void* entity);

// The tile of the other end of an underground belt or pipe to ground, when it lies in the 16-way
// `direction` from the entity, and how many tiles away; a skip jumps to it.
struct UndergroundEnd {
   int32_t tileX;
   int32_t tileY;
   int tiles;
};
std::optional<UndergroundEnd> undergroundEnd(const void* entity, uint8_t direction);

} // namespace fa::facts
