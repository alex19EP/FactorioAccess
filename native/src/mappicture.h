#pragma once

#include "mapcolours.h"

#include <cstdint>
#include <optional>

// The map picture: what a force's chart shows on a tile, where remote view and the map draw the
// chart instead of the live world (a chunk under fog of war). The chart keeps colours, not things:
// one RGB565 pixel a tile, the tile's map colour with each entity drawn over it as it stood when
// the chunk was last charted (Chart::chartChunkInternal, Chart::drawEntity). So the picture is
// told back from the colour, as a sighted player tells iron ore from a belt on the map: the tile,
// resource or kind of building whose map colour it is. A colour the game gives a whole type (every
// belt tier) names the type's first prototype by order; the default building colour, enemies and
// ghosts have words of their own (Kind). The matching itself is in mapcolours.h.
namespace fa::mappicture {

// What the chart `chart` (ForceData::getChart) shows on the tile `tileX`, `tileY`: nothing when
// its chunk is not charted; a picture of nothing when the colour is none the game draws with (two
// things blended, a building marked for deconstruction). Several tiles share a map colour; the
// ground seldom changes, so where the tile there now (`tile`, a TilePrototype) has the pixel's
// colour, it is that tile. Where the game translates and draws, as the reader's Gui tick does.
struct Shown {
   std::optional<Picture> picture;
};
std::optional<Shown> at(const void* chart, int32_t tileX, int32_t tileY, const void* tile);

} // namespace fa::mappicture
