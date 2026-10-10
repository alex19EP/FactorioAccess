#pragma once

#include <cstdint>
#include <functional>
#include <optional>

// The skip keys (Shift+WASD): where the cursor stops when it skips over alike tiles in a direction
// (control.lua's cursor_skip_iteration, now read here as the key is pressed). Each tile is judged
// by what the game would select under the mouse there (reader.h), as the tile reader names it;
// robots and corpses are passed over. The cursor stops on the first tile that differs from where
// it started: another entity, or the same kind facing elsewhere, a belt fed or feeding otherwise or
// turning otherwise, ground after an entity or water after ground. From an underground belt or pipe
// to ground it jumps to the other end when that lies ahead; from water it stops on the first tile
// that is not water. An audio ruler stops it where it comes into or out of line with the ruler, and
// on the ruler's own tile. Where the view shows fog of war or nothing (remote view, the map), it
// goes by what the chart's picture shows (reader::shownAt) and stops where that changes.
namespace fa::skip {

// The player's audio ruler, a tile.
struct Ruler {
   int32_t x;
   int32_t y;
};

struct Result {
   enum class Kind {
      Found, // a change at `x`, `y`, `tiles` away
      Limit, // no change within the limit; the cursor goes on to `x`, `y`
      Edge,  // the next tile is one the cursor may not go to: it stays at `x`, `y`
   };
   Kind kind;
   int32_t x;
   int32_t y;
   int tiles;
};

// From the tile `x`, `y` in the direction `dx`, `dy` (one of them 1 or -1), going at most `limit`
// tiles, onto tiles `allowed` lets the cursor go to. In the game's input handling.
Result find(int32_t x, int32_t y, int32_t dx, int32_t dy, int limit,
            const std::function<bool(int32_t, int32_t)>& allowed, std::optional<Ruler> ruler);

} // namespace fa::skip
