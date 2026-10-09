#include "facts.h"

#include "game.h"
#include "vocab.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace fa::facts {

namespace {

using game::layout;

template <class T>
T at(const void* object, uint32_t offset) {
   T value;
   std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
   return value;
}

template <class Function>
Function virtualAt(const void* object, uint32_t slot) {
   auto vtable = *static_cast<void* const* const*>(object);
   return reinterpret_cast<Function>(vtable[slot]);
}

// MapPosition: two fixed-point ints.
struct Position {
   int32_t x;
   int32_t y;
};

// Directions as the game and defines.direction count them: 16, north 0 clockwise.
constexpr uint8_t kNorth = 0;
constexpr uint8_t kEast = 4;
constexpr uint8_t kSouth = 8;
constexpr uint8_t kWest = 12;
constexpr uint8_t kStraight[4] = {kNorth, kEast, kSouth, kWest};

uint8_t turned(uint8_t direction, int eighths) { return static_cast<uint8_t>((direction + eighths * 2 + 16) % 16); }
uint8_t clockwise(uint8_t direction) { return turned(direction, 2); }
uint8_t counterclockwise(uint8_t direction) { return turned(direction, -2); }
uint8_t opposite(uint8_t direction) { return turned(direction, 4); }

std::string directionName(uint8_t direction) { return vocab::kDirection(direction); }

std::string_view typeName(const void* prototype) {
   return virtualAt<const char* (*)(const void*)>(prototype, layout.prototypeGetType)(prototype);
}

const std::byte* prototypeOf(const void* entity) { return at<const std::byte*>(entity, layout.entityPrototypeOf); }

// Entity::usageBitMask bits of entities the Lua API never hands out, which belt_neighbours skips
// (LuaEntity::luaReadBeltNeighbours).
constexpr uint16_t kNotListedBit = 0x10;

// ---- facing ----

std::string facing(const std::byte* entity) {
   const std::string_view type = typeName(prototypeOf(entity));
   // Rails have their own description.
   if (type == "straight-rail" || type == "half-diagonal-rail" || type == "curved-rail-a" || type == "curved-rail-b")
      return {};
   // A ghost faces as the building it stands for.
   const std::byte* building = type == "entity-ghost" ? at<const std::byte*>(entity, layout.scanGhostInner) : entity;
   if (!building) return {};
   const std::byte* prototype = prototypeOf(building);
   if (!virtualAt<bool (*)(const void*)>(prototype, layout.prototypeIsBuilding)(prototype) ||
       !virtualAt<bool (*)(const void*)>(building, layout.entityHasDirection)(building))
      return {};
   uint8_t direction = 0;
   virtualAt<void* (*)(const void*, uint8_t*)>(building, layout.entityGetDirection)(building, &direction);
   // A steam engine or turbine works the same both ways along its axis.
   if (typeName(prototype) == "generator")
      return vocab::kFacingSymmetric(directionName(direction), directionName(opposite(direction)));
   return vocab::kFacing(directionName(direction));
}

// ---- belts ----

// A belt connectable (a belt, underground belt, splitter, loader ...), which starts with its
// Entity. Known by its prototype's type, as the mod's Lua knows it.
struct Belt {
   const std::byte* entity = nullptr;

   explicit operator bool() const { return entity != nullptr; }
   bool operator==(const Belt& other) const { return entity == other.entity; }

   std::string_view type() const { return typeName(prototypeOf(entity)); }
   uint8_t direction() const { return at<uint8_t>(entity, layout.beltDirection); }
   bool isTransportBelt() const { return type() == "transport-belt"; }
   bool isUnderground() const { return type() == "underground-belt"; }
   // A transport belt's corner: Straight, Left or Right; none of them for anything else.
   uint32_t shape() const { return isTransportBelt() ? at<uint8_t>(entity, layout.beltShape) : ~0u; }
   bool isUndergroundExit() const {
      return isUnderground() && at<uint8_t>(entity, layout.undergroundType) == layout.undergroundOutput;
   }
   Belt partner() const {
      return Belt{isUnderground() ? at<const std::byte*>(entity, layout.undergroundPartner) : nullptr};
   }
};

// Lua's belt_neighbours: the belts on each side that feed this one, and the one it feeds.
struct Neighbours {
   Belt inputs[8];
   size_t inputCount = 0;
   Belt outputs[8];
   size_t outputCount = 0;
};

// A splitter's other half has its own neighbours too (Splitter::secondaryConnections), which these
// facts never ask for: only belts and underground belts have a shape.
Neighbours neighboursOf(Belt belt) {
   Neighbours out;
   const uint8_t own = belt.direction();
   for (size_t i = 0; i < 4; ++i) {
      const Belt other{at<const std::byte*>(belt.entity, layout.beltNeighbours[i])};
      if (!other || other == belt || (at<uint16_t>(other.entity, layout.entityUsageBits) & kNotListedBit)) continue;
      const uint8_t side = kStraight[i];
      if (side == own) {
         if (other.direction() != opposite(own) && out.outputCount < std::size(out.outputs))
            out.outputs[out.outputCount++] = other;
      } else if (other.direction() == opposite(side) && out.inputCount < std::size(out.inputs)) {
         out.inputs[out.inputCount++] = other;
      }
   }
   return out;
}

// The way items come in: a corner's comes from its side.
uint8_t inputDirection(Belt belt) {
   const uint32_t shape = belt.shape();
   if (shape == layout.beltShapeLeft) return clockwise(belt.direction());
   if (shape == layout.beltShapeRight) return counterclockwise(belt.direction());
   return belt.direction();
}

// The words for a transport belt's or an underground belt's shape (scripts/transport-belts.lua,
// Node:get_shape_info, and fa-info.lua's ent_info_belt_shape).
std::vector<std::string> beltShape(Belt belt) {
   const Neighbours neighbours = neighboursOf(belt);
   const uint8_t outgoing = belt.direction();

   // The belts behind and to each side that feed it.
   Belt behind;
   Belt left;
   Belt right;
   if (neighbours.inputCount == 1 && neighbours.inputs[0].direction() == outgoing) {
      behind = neighbours.inputs[0];
   } else {
      const uint8_t incoming = inputDirection(belt);
      for (size_t i = 0; i < neighbours.inputCount; ++i) {
         const Belt input = neighbours.inputs[i];
         if (input.direction() == incoming)
            behind = input;
         else if (input.direction() == clockwise(incoming))
            left = input;
         else if (input.direction() == counterclockwise(incoming))
            right = input;
      }
   }
   if (belt.isUndergroundExit()) behind = belt.partner();

   // An entrance feeds its exit.
   Belt child = neighbours.outputCount > 0 ? neighbours.outputs[0] : Belt{};
   if (!child && belt.isUnderground() && !belt.isUndergroundExit()) child = belt.partner();

   const bool hasInput = behind || left || right;
   const bool hasOutput = static_cast<bool>(child);
   // It pours onto the side of the belt it feeds.
   const bool pouring = hasOutput && outgoing != inputDirection(child);
   const bool merge = belt.isTransportBelt() && left && right && !behind;

   std::vector<std::string> out;
   const uint32_t shape = belt.shape();
   // "From the": the side it turns from.
   if (shape == layout.beltShapeLeft)
      out.push_back(vocab::kBeltLeftTurn(directionName(counterclockwise(outgoing))));
   else if (shape == layout.beltShapeRight)
      out.push_back(vocab::kBeltRightTurn(directionName(clockwise(outgoing))));

   if (merge)
      out.emplace_back(vocab::kBeltMerge);
   else if (left && right)
      out.emplace_back(vocab::kBeltDoubleSideload);
   else if (left)
      out.emplace_back(vocab::kBeltLeftSideload);
   else if (right)
      out.emplace_back(vocab::kBeltRightSideload);

   if (!hasInput && !hasOutput)
      out.emplace_back(vocab::kBeltUnit);
   else if (pouring)
      out.emplace_back(vocab::kBeltPouring);
   else if (hasInput && !hasOutput)
      out.emplace_back(vocab::kBeltStop);
   else if (hasOutput && !hasInput)
      out.emplace_back(vocab::kBeltStart);
   return out;
}

// Where an underground belt's partner is: straight ahead or behind, so one of four directions.
std::string undergroundConnection(Belt belt) {
   const Belt partner = belt.partner();
   if (!partner) return vocab::kUndergroundNotConnected;
   const Position from = at<Position>(belt.entity, layout.entityPosition);
   const Position to = at<Position>(partner.entity, layout.entityPosition);
   const int32_t dx = to.x - from.x;
   const int32_t dy = to.y - from.y;
   uint8_t direction = kNorth;
   if (std::abs(dx) > std::abs(dy))
      direction = dx > 0 ? kEast : kWest;
   else
      direction = dy > 0 ? kSouth : kNorth;
   const int32_t tiles = (std::abs(dx) + std::abs(dy)) / game::kMapPositionScale;
   return vocab::kUndergroundConnection(directionName(direction), tiles);
}

std::string joined(const std::vector<std::string>& words) {
   std::string out;
   for (const std::string& word : words) {
      if (word.empty()) continue;
      if (!out.empty()) out += ' ';
      out += word;
   }
   return out;
}

} // namespace

std::vector<std::string> describe(const void* object) {
   const auto* entity = static_cast<const std::byte*>(object);
   const Belt candidate{entity};
   const Belt belt = candidate.isTransportBelt() || candidate.isUnderground() ? candidate : Belt{};

   // One list item, as the mod's Lua said them: facing, an underground belt's end, a belt's shape.
   std::vector<std::string> first{facing(entity)};
   if (belt && belt.isUnderground())
      first.emplace_back(belt.isUndergroundExit() ? vocab::kUndergroundExit : vocab::kUndergroundEntrance);
   if (belt)
      for (std::string& word : beltShape(belt)) first.push_back(std::move(word));

   std::vector<std::string> parts{joined(first)};
   if (belt && belt.isUnderground()) parts.push_back(undergroundConnection(belt));
   return parts;
}

} // namespace fa::facts
