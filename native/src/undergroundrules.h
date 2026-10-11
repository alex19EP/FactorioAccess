#pragma once

// Which underground belt an underground belt pairs with, as the game pairs them when one is built
// (UndergroundBelt::findConnectionCandidateOrBlocker): from its tile along the way items flow
// through it (an entrance's facing, an exit's opposite), up to its prototype's reach, the first
// underground belt of its own prototype decides. One whose items flow the other way is its partner;
// one whose flow the same way blocks it, and it pairs with none. Those across the line are passed
// over. A blueprint leaves its underground belts unpaired, so facts.cpp pairs them by this. Pure,
// so the tests can pair belts of their own.

#include <cstdint>

namespace fa::undergroundrules {

// Directions as defines.direction counts them: 16, north 0 clockwise.
inline uint8_t reversed(uint8_t flow) { return static_cast<uint8_t>((flow + 8) % 16); }

// The partner of the underground belt on the tile `x`, `y` whose items flow `flow` (north, east,
// south or west), within `reach` tiles; a false Entity for none. `sameKindAt(x, y)` is the
// underground belt of its prototype on a tile, or a false Entity, and `flowOf` the way items flow
// through one.
template <typename Entity, typename SameKindAt, typename FlowOf>
Entity partner(int32_t x, int32_t y, uint8_t flow, int reach, const SameKindAt& sameKindAt, const FlowOf& flowOf) {
   const int32_t dx = flow == 4 ? 1 : flow == 12 ? -1 : 0;
   const int32_t dy = flow == 8 ? 1 : flow == 0 ? -1 : 0;
   for (int step = 1; step <= reach; ++step) {
      const Entity other = sameKindAt(x + dx * step, y + dy * step);
      if (!other) continue;
      const uint8_t otherFlow = flowOf(other);
      if (otherFlow == flow) return Entity{};
      if (otherFlow == reversed(flow)) return other;
   }
   return Entity{};
}

} // namespace fa::undergroundrules
