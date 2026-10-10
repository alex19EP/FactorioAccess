#pragma once

// The cycle key's stops in a tile (reader.h, cursor.h), from the selection boxes reaching into it
// and what the mouse selects at a point. Pure, so the tests can cycle through boxes of their own.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <vector>

namespace fa::cyclestops {

// MapPosition's fixed point: 256 to a tile (game::kMapPositionScale).
constexpr int32_t kScale = 256;

// A selection box, in fixed point.
struct Box {
   int32_t left;
   int32_t top;
   int32_t right;
   int32_t bottom;
};

template <typename Entity>
struct Stop {
   Entity entity;
   int32_t x;
   int32_t y;
};

// The stops in the tile `tileX`, `tileY`: each entity the mouse can select somewhere in it, with
// the point nearest the tile's centre where it does; first what the centre selects, the rest in
// the order `before(a, b)` puts them (true when a takes the mouse from b); last the bare ground (a
// false Entity), where some point selects nothing. `boxes` are the selection boxes near the tile,
// and `selectAt(x, y)` what the mouse selects at a point.
template <typename Entity, typename SelectAt, typename Before>
std::vector<Stop<Entity>> stopsIn(int32_t tileX, int32_t tileY, const std::vector<Box>& boxes, const SelectAt& selectAt,
                                  const Before& before) {
   struct Point {
      int32_t x;
      int32_t y;
   };
   const int32_t left = tileX * kScale, top = tileY * kScale, right = left + kScale, bottom = top + kScale;
   const Point centre{left + kScale / 2, top + kScale / 2};

   // The selection boxes reaching into the tile cut it into cells, and the same boxes hold every
   // point of a cell, so a point in each cell meets everything the mouse can select here.
   std::vector<int32_t> xs{left, right}, ys{top, bottom};
   for (const Box& box : boxes) {
      if (box.right <= left || box.left >= right || box.bottom <= top || box.top >= bottom) continue;
      xs.push_back(std::clamp(box.left, left, right));
      xs.push_back(std::clamp(box.right, left, right));
      ys.push_back(std::clamp(box.top, top, bottom));
      ys.push_back(std::clamp(box.bottom, top, bottom));
   }
   for (std::vector<int32_t>* edges : {&xs, &ys}) {
      std::ranges::sort(*edges);
      edges->erase(std::ranges::unique(*edges).begin(), edges->end());
   }
   std::vector<Point> points{centre};
   for (size_t i = 0; i + 1 < xs.size(); ++i)
      for (size_t j = 0; j + 1 < ys.size(); ++j) points.push_back({(xs[i] + xs[i + 1]) / 2, (ys[j] + ys[j + 1]) / 2});
   const auto distance = [&](Point point) {
      const int64_t dx = point.x - centre.x, dy = point.y - centre.y;
      return dx * dx + dy * dy;
   };
   std::ranges::stable_sort(points, {}, distance);

   std::vector<Stop<Entity>> stops;
   std::optional<Stop<Entity>> ground;
   for (const Point point : points) {
      const Entity entity = selectAt(point.x, point.y);
      if (!entity) {
         if (!ground) ground = Stop<Entity>{entity, point.x, point.y};
      } else if (std::ranges::find(stops, entity, &Stop<Entity>::entity) == stops.end()) {
         stops.push_back(Stop<Entity>{entity, point.x, point.y});
      }
   }
   if (stops.size() > 2)
      std::stable_sort(stops.begin() + 1, stops.end(),
                       [&](const Stop<Entity>& a, const Stop<Entity>& b) { return before(a.entity, b.entity); });
   if (ground) stops.push_back(*ground);
   return stops;
}

// The stop the cycle key goes to from the one the mouse selects now: the next, the first after the
// last, and the first when what is selected is none of them.
template <typename Entity>
size_t nextStop(const std::vector<Stop<Entity>>& stops, const Entity& selected) {
   const auto shown = std::ranges::find(stops, selected, &Stop<Entity>::entity);
   return shown == stops.end() ? 0 : (static_cast<size_t>(shown - stops.begin()) + 1) % stops.size();
}

} // namespace fa::cyclestops
