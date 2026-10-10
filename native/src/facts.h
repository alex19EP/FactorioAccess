#pragma once

#include <cstdint>
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

} // namespace fa::facts
