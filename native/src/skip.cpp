#include "skip.h"

#include "facts.h"
#include "reader.h"

namespace fa::skip {

namespace {

struct Tile {
   int32_t x;
   int32_t y;
};

// Directions as defines.direction counts them: 16, north 0 clockwise.
uint8_t directionOf(int32_t dx, int32_t dy) {
   if (dx > 0) return 4;
   if (dx < 0) return 12;
   return dy > 0 ? 8 : 0;
}

// The tile's entity as the skip sees it: robots and corpses are not there.
const void* entityOn(Tile tile) {
   const void* entity = reader::selectedAt(tile.x, tile.y);
   return entity && facts::skipsOver(entity) ? nullptr : entity;
}

} // namespace

Result find(int32_t x, int32_t y, int32_t dx, int32_t dy, int limit,
            const std::function<bool(int32_t, int32_t)>& allowed, std::optional<Ruler> ruler) {
   const Tile origin{x, y};
   // In line with the ruler: on its row or column (Rulers.is_any_ruler_aligned).
   auto aligned = [&](Tile tile) { return ruler && (tile.x == ruler->x || tile.y == ruler->y); };
   auto atRuler = [&](Tile tile) { return ruler && tile.x == ruler->x && tile.y == ruler->y; };
   auto rulerStops = [&](Tile tile) { return aligned(tile) != aligned(origin) || atRuler(tile); };
   auto next = [&](Tile tile) { return Tile{tile.x + dx, tile.y + dy}; };
   auto may = [&](Tile tile) { return allowed(tile.x, tile.y); };

   // Under fog of war the view shows the chart's picture, and the mouse selects nothing there: the
   // skip goes by the picture, and stops where it changes, where the fog ends or where it begins.
   const std::optional<reader::Shown> startShown = reader::shownAt(x, y);
   const void* start = startShown ? nullptr : entityOn(origin);
   if (start) {
      if (const auto end = facts::undergroundEnd(start, directionOf(dx, dy))) {
         if (!may(Tile{end->tileX, end->tileY})) return {Result::Kind::Edge, x, y, 0};
         return {Result::Kind::Found, end->tileX, end->tileY, end->tiles};
      }
   }

   Tile tile = next(origin);
   if (!may(tile)) return {Result::Kind::Edge, x, y, 0};
   int moved = 1;

   // From water, the first tile that is not water.
   if (!startShown && !start && reader::waterAt(x, y)) {
      while (moved < limit) {
         if (!reader::waterAt(tile.x, tile.y) || rulerStops(tile)) return {Result::Kind::Found, tile.x, tile.y, moved};
         const Tile further = next(tile);
         if (!may(further)) return {Result::Kind::Edge, tile.x, tile.y, moved};
         tile = further;
         ++moved;
      }
      return {Result::Kind::Limit, tile.x, tile.y, moved};
   }

   const std::optional<facts::Likeness> startLikeness =
      start ? std::optional<facts::Likeness>(facts::likenessOf(start)) : std::nullopt;
   while (moved < limit) {
      if (rulerStops(tile)) return {Result::Kind::Found, tile.x, tile.y, moved};
      const std::optional<reader::Shown> shown = reader::shownAt(tile.x, tile.y);
      const void* current = shown ? nullptr : reader::selectedAt(tile.x, tile.y);
      bool changed = false;
      if (shown || startShown) {
         changed = shown != startShown;
      } else if (current && facts::skipsOver(current)) {
         // A robot or corpse hides what is under it: go on over it.
      } else if (!current) {
         // Ground after an entity, or water after ground.
         changed = start || reader::waterAt(tile.x, tile.y);
      } else {
         changed = !start || (current != start && facts::likenessOf(current) != *startLikeness);
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
