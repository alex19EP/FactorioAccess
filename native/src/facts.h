#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
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

// A blueprint's entities, which lie on no surface. The game links its belts, pipes and heat pipes
// among themselves (Entity::blueprintSetup), but not its underground belt pairs nor what an
// inserter or a drill reaches: those are found with `entityAt`, what the blueprint's picture would
// pick at a map position (fixed point, the blueprint's own coordinates) among the entities it
// keeps, or null. The links stay to an entity the player removed from the blueprint, which is not
// built: `keeps` tells it apart.
struct Design {
   std::function<const void*(int32_t x, int32_t y)> entityAt;
   std::function<bool(const void* entity)> keeps;
};

// The facts of a blueprint's entity that tell of its design, as describe says them: none of the
// world's state (what a belt carries, power, networks, trains).
std::vector<std::string> describeDesign(const void* entity, int32_t tileX, int32_t tileY, const Design& design);

// What the info icons the game draws on `entity` show (alt mode), in FA's words where it has them:
// the recipe as "producing", filters as "Filters for" or
// "Denies", fluids as "Has", a beacon's modules with counts, any other icon by name. Each part a
// list item; none while the game draws no icons. In the application Gui's logic. Drawn as the main
// view draws, or with `renderParameters` (a RenderParameters*), such as a blueprint picture's own.
std::vector<std::string> drawnIcons(const void* entity);
std::vector<std::string> drawnIcons(const void* entity, const void* renderParameters);

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

// Whether a step from the tile where the mouse selects `left` onto the one where it selects
// `entered` (either may be null) goes from an entity bigger than a tile onto another of its kind:
// in a row of machines alike the reader says the same name on each, and a sound marks where the
// next one begins.
bool nextOfKind(const void* left, const void* entered);

// The tile of the other end of an underground belt or pipe to ground, when it lies in the 16-way
// `direction` from the entity, and how many tiles away; a skip jumps to it. With `design`, in a
// blueprint.
struct UndergroundEnd {
   int32_t tileX;
   int32_t tileY;
   int tiles;
};
std::optional<UndergroundEnd> undergroundEnd(const void* entity, uint8_t direction, const Design* design = nullptr);

// What a mining drill mines from: the prototypes of the resources in its mining area
// (MiningDrill::resourcesToMine), each once, by internal name. The tile reader says them, and the
// scanner groups drills by them.
std::vector<const void*> drillResources(const void* drill);

// The entities on `surface` whose box overlaps the area from (left, top) to (right, bottom), map
// positions in the game's fixed point, as EntitySearch<Entity> finds them.
std::vector<const void*> entitiesIn(const void* surface, int32_t left, int32_t top, int32_t right, int32_t bottom);

// The box the mouse selects `entity` in (Entity::getSelectionBox), in fixed point; a vehicle's as
// if unturned.
struct SelectionBox {
   int32_t left;
   int32_t top;
   int32_t right;
   int32_t bottom;
};
SelectionBox selectionBox(const void* entity);

} // namespace fa::facts
