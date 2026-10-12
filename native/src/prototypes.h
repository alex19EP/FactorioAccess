#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace fa::prototypes {

// The localised name, in the game's current locale, of the prototype that rich text names as
// `kind` and `name`: a tag ([item=iron-plate]) or a sprite path (item/iron-plate). Nothing when
// no such prototype exists or `kind` names none (utility sprites, GPS tags).
std::optional<std::string> localisedName(std::string_view kind, std::string_view name);

// A prototype as rich text names it: its kind ("item", "virtual-signal") and internal name.
struct Named {
   std::string_view kind;
   std::string name;
};
// Nothing when `prototype` is not one of the kinds rich text names.
std::optional<Named> identify(const void* prototype);

// What a tile is to the cursor and the scanner, by the collision layers the game gives it
// (data/base/prototypes/tile/tile-collision-masks.lua), so that modded tiles take part as theirs
// say. Water-like tiles have the water_tile layer without ground_tile, which leaves out empty space
// and out-of-map.
enum class TileKind : uint8_t {
   Land,
   Liquid,  // water-like and blocks walking: water, deep water, lava, ammoniacal ocean, brash ice
   Shallow, // water-like the player walks on, still not built on: shallow water, Gleba's wetlands, oil ocean
   Ice,     // meltable: Aquilo's ice, which melts under what is built on it unheated
};
TileKind tileKind(const void* tilePrototype);

// Whether a tile is water-like: a liquid or shallow.
inline bool waterLike(TileKind kind) { return kind == TileKind::Liquid || kind == TileKind::Shallow; }

// The fluid an offshore pump pumps from a tile, by ID; 0 for none.
uint16_t tileFluid(const void* tilePrototype);

} // namespace fa::prototypes
