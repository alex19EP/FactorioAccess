#pragma once

// Where a skip stops (skip.h), over a world that says what each tile holds. Pure, so the tests can
// skip over a world of their own.

#include <concepts>
#include <cstdint>
#include <optional>

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

// A world's Entity is false for none. selectedAt is what the mouse selects on a tile, waterAt
// whether the tile is water, and shownAt what the chart's picture shows there where the view shows
// it instead of the live world (fog of war, the map), compared with ==. skipsOver is true for
// robots and corpses, likenessOf what makes two entities alike, compared with ==, and undergroundEnd
// the other end of an underground belt or pipe to ground lying in a 16-way direction, with its tile
// (tileX, tileY) and how many tiles away it is (tiles).
template <typename World>
concept SkipWorld = requires(const World& world, typename World::Entity entity, int32_t x, uint8_t direction) {
   { world.selectedAt(x, x) } -> std::convertible_to<typename World::Entity>;
   { world.waterAt(x, x) } -> std::convertible_to<bool>;
   { world.shownAt(x, x) != world.shownAt(x, x) } -> std::convertible_to<bool>;
   { world.skipsOver(entity) } -> std::convertible_to<bool>;
   { world.likenessOf(entity) != world.likenessOf(entity) } -> std::convertible_to<bool>;
   world.undergroundEnd(entity, direction);
};

namespace detail {

// Directions as defines.direction counts them: 16, north 0 clockwise.
inline uint8_t directionOf(int32_t dx, int32_t dy) {
   if (dx > 0) return 4;
   if (dx < 0) return 12;
   return dy > 0 ? 8 : 0;
}

} // namespace detail

// From the tile `x`, `y` in the direction `dx`, `dy` (one of them 1 or -1), going at most `limit`
// tiles, onto tiles `allowed(x, y)` lets the cursor go to.
template <SkipWorld World, typename Allowed>
Result find(const World& world, int32_t x, int32_t y, int32_t dx, int32_t dy, int limit, const Allowed& allowed,
            std::optional<Ruler> ruler) {
   struct Tile {
      int32_t x;
      int32_t y;
   };
   using Entity = typename World::Entity;
   const Tile origin{x, y};
   // In line with the ruler: on its row or column (Rulers.is_any_ruler_aligned).
   auto aligned = [&](Tile tile) { return ruler && (tile.x == ruler->x || tile.y == ruler->y); };
   auto atRuler = [&](Tile tile) { return ruler && tile.x == ruler->x && tile.y == ruler->y; };
   auto rulerStops = [&](Tile tile) { return aligned(tile) != aligned(origin) || atRuler(tile); };
   auto next = [&](Tile tile) { return Tile{tile.x + dx, tile.y + dy}; };
   auto may = [&](Tile tile) { return allowed(tile.x, tile.y); };
   // The tile's entity as the skip sees it: robots and corpses are not there.
   auto entityOn = [&](Tile tile) -> Entity {
      const Entity entity = world.selectedAt(tile.x, tile.y);
      return entity && world.skipsOver(entity) ? Entity{} : entity;
   };

   // Under fog of war the view shows the chart's picture, and the mouse selects nothing there: the
   // skip goes by the picture, and stops where it changes, where the fog ends or where it begins.
   const auto startShown = world.shownAt(x, y);
   const Entity start = startShown ? Entity{} : entityOn(origin);
   if (start) {
      if (const auto end = world.undergroundEnd(start, detail::directionOf(dx, dy))) {
         if (!may(Tile{end->tileX, end->tileY})) return {Result::Kind::Edge, x, y, 0};
         return {Result::Kind::Found, end->tileX, end->tileY, end->tiles};
      }
   }

   Tile tile = next(origin);
   if (!may(tile)) return {Result::Kind::Edge, x, y, 0};
   int moved = 1;

   // From water, the first tile that is not water.
   if (!startShown && !start && world.waterAt(x, y)) {
      while (moved < limit) {
         if (!world.waterAt(tile.x, tile.y) || rulerStops(tile)) return {Result::Kind::Found, tile.x, tile.y, moved};
         const Tile further = next(tile);
         if (!may(further)) return {Result::Kind::Edge, tile.x, tile.y, moved};
         tile = further;
         ++moved;
      }
      return {Result::Kind::Limit, tile.x, tile.y, moved};
   }

   using Likeness = decltype(world.likenessOf(start));
   const std::optional<Likeness> startLikeness =
      start ? std::optional<Likeness>(world.likenessOf(start)) : std::nullopt;
   while (moved < limit) {
      if (rulerStops(tile)) return {Result::Kind::Found, tile.x, tile.y, moved};
      const auto shown = world.shownAt(tile.x, tile.y);
      const Entity current = shown ? Entity{} : world.selectedAt(tile.x, tile.y);
      bool changed = false;
      if (shown || startShown) {
         changed = shown != startShown;
      } else if (current && world.skipsOver(current)) {
         // A robot or corpse hides what is under it: go on over it.
      } else if (!current) {
         // Ground after an entity, or water after ground.
         changed = start || world.waterAt(tile.x, tile.y);
      } else {
         changed = !start || (current != start && world.likenessOf(current) != *startLikeness);
      }
      if (changed) return {Result::Kind::Found, tile.x, tile.y, moved};
      const Tile further = next(tile);
      if (!may(further)) return {Result::Kind::Edge, tile.x, tile.y, moved};
      tile = further;
      ++moved;
   }
   return {Result::Kind::Limit, tile.x, tile.y, moved};
}

} // namespace fa::skip
