#pragma once

// A rail's kind as the tile reader says it, the end of an fa.rails-descriptions-* key. Straight and
// half-diagonal rails go by direction. A curve is the part of a whole 90 degree turn it is, or left
// or right of a direction while the turn is not all built. Pure over a rail graph, so the tests can
// walk a graph built from the game's rail data.

#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <string>

namespace fa::railkinds {

enum class RailType : uint8_t { Straight, HalfDiagonal, CurveA, CurveB };

constexpr int kFront = 0;
constexpr int kBack = 1;
constexpr int kLeft = -1;
constexpr int kStraightOn = 0;
constexpr int kRight = 1;

// A graph's Rail is false for no rail. placement is the rail's direction and endDirection the way
// an end faces, both 0 to 15. linked is the rail joined to an end, turning left, straight on or
// right.
template <typename Graph>
concept RailGraph = requires(const Graph& graph, typename Graph::Rail rail) {
   { graph.placement(rail) } -> std::convertible_to<uint8_t>;
   { graph.endDirection(rail, kFront) } -> std::convertible_to<uint8_t>;
   { graph.linked(rail, kFront, kLeft) } -> std::convertible_to<typename Graph::Rail>;
};

// The end of `next` that is not joined to `from`: where a walk from `from` goes on.
template <RailGraph Graph>
int farEnd(const Graph& graph, typename Graph::Rail next, typename Graph::Rail from) {
   for (const int turn : {kLeft, kStraightOn, kRight})
      if (graph.linked(next, kFront, turn) == from) return kBack;
   return kFront;
}

template <RailGraph Graph>
int endFacing(const Graph& graph, typename Graph::Rail rail, uint8_t direction) {
   return graph.endDirection(rail, kBack) == direction ? kBack : kFront;
}

namespace detail {

// For a curve, which part of a 90 degree turn it is, and the turns to walk from either end to find
// the rest of the turn.
struct TurnWalk {
   uint8_t end;
   int turn;
   int count;
};
struct Turn {
   const char* position;
   uint8_t from;
   uint8_t to;
   TurnWalk forward;
   TurnWalk back;
};
// By curve (a, b) and placement direction / 2.
constexpr Turn kTurns[2][8] = {
   {
      {"bottom", 4, 8, {8, 1, 0}, {15, -1, 3}},
      {"bottom", 12, 8, {8, -1, 0}, {1, 1, 3}},
      {"bottom", 4, 0, {3, -1, 3}, {12, 1, 0}},
      {"top", 4, 8, {5, 1, 3}, {12, -1, 0}},
      {"top", 12, 0, {0, 1, 0}, {7, -1, 3}},
      {"top", 4, 0, {0, -1, 0}, {9, 1, 3}},
      {"top", 12, 8, {11, -1, 3}, {4, 1, 0}},
      {"bottom", 12, 0, {13, 1, 3}, {4, -1, 0}},
   },
   {
      {"lower-half", 4, 8, {7, 1, 1}, {14, -1, 2}},
      {"lower-half", 12, 8, {9, -1, 1}, {2, 1, 2}},
      {"lower-half", 4, 0, {2, -1, 2}, {11, 1, 1}},
      {"upper-half", 4, 8, {6, 1, 2}, {13, -1, 1}},
      {"upper-half", 12, 0, {15, 1, 1}, {6, -1, 2}},
      {"upper-half", 4, 0, {1, -1, 1}, {10, 1, 2}},
      {"upper-half", 12, 8, {10, -1, 2}, {3, 1, 1}},
      {"lower-half", 12, 0, {14, 1, 2}, {5, -1, 1}},
   },
};
// The kind a curve is said as when no whole turn is built: left or right of a cardinal (a) or a
// diagonal (b), by placement direction / 2.
constexpr const char* kCurveFallback[2][8] = {
   {"left-of-north", "right-of-north", "left-of-east", "right-of-east", "left-of-south", "right-of-south",
    "left-of-west", "right-of-west"},
   {"right-of-southeast", "left-of-southwest", "right-of-southwest", "left-of-northwest", "right-of-northwest",
    "left-of-northeast", "right-of-northeast", "left-of-southeast"},
};
// A straight or half-diagonal rail's kind, by its end direction mod 8.
constexpr const char* kSimpleKinds[8] = {"vertical",   "northnortheast", "northeast", "eastnortheast",
                                         "horizontal", "eastsoutheast",  "southeast", "southsoutheast"};

inline const char* cardinalName(uint8_t direction) {
   switch (direction) {
   case 0: return "north";
   case 4: return "east";
   case 8: return "south";
   default: return "west";
   }
}

template <RailGraph Graph>
bool walkTurn(const Graph& graph, typename Graph::Rail rail, const TurnWalk& walk) {
   typename Graph::Rail current = rail;
   int end = endFacing(graph, rail, walk.end);
   for (int i = 0; i < walk.count; ++i) {
      const typename Graph::Rail next = graph.linked(current, end, walk.turn > 0 ? kRight : kLeft);
      if (!next) return false;
      end = farEnd(graph, next, current);
      current = next;
   }
   return true;
}

} // namespace detail

// The rail's kind ("horizontal", "bottom-of-east-to-south-turn"). Turns are found only when
// `walk`, through the rail graph.
template <RailGraph Graph>
std::string kindOf(const Graph& graph, typename Graph::Rail rail, RailType type, bool walk) {
   if (type == RailType::Straight || type == RailType::HalfDiagonal)
      return detail::kSimpleKinds[graph.endDirection(rail, kFront) % 8];
   const int curve = type == RailType::CurveA ? 0 : 1;
   const size_t index = (graph.placement(rail) / 2u) % 8u;
   const detail::Turn& turn = detail::kTurns[curve][index];
   if (walk && detail::walkTurn(graph, rail, turn.forward) && detail::walkTurn(graph, rail, turn.back))
      return std::string(turn.position) + "-of-" + detail::cardinalName(turn.from) + "-to-" +
             detail::cardinalName(turn.to) + "-turn";
   return detail::kCurveFallback[curve][index];
}

} // namespace fa::railkinds
