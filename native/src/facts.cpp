#include "facts.h"

#include "agui.h"
#include "entityicons.h"
#include "game.h"
#include "prototypes.h"
#include "text.h"
#include "vocab.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <utility>

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

// A prototype's internal name ("nuclear-reactor"), as Lua's name reads it.
std::string_view internalName(const std::byte* prototype) {
   struct MsvcString {
      union {
         char buffer[16];
         const char* pointer;
      };
      size_t size;
      size_t capacity;
   };
   const auto& name = *reinterpret_cast<const MsvcString*>(prototype + layout.prototypeName);
   return {name.capacity >= sizeof(name.buffer) ? name.pointer : name.buffer, name.size};
}

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

// A splitter's other half has its own neighbours (Splitter::secondaryConnections), counted too.
Neighbours neighboursOf(Belt belt) {
   Neighbours out;
   const uint8_t own = belt.direction();
   auto add = [&](const uint32_t (&sides)[4]) {
      for (size_t i = 0; i < 4; ++i) {
         const Belt other{at<const std::byte*>(belt.entity, sides[i])};
         if (!other || other == belt || (at<uint16_t>(other.entity, layout.entityUsageBits) & kNotListedBit)) continue;
         const uint8_t side = kStraight[i];
         if (side == own) {
            if (other.direction() != opposite(own) && out.outputCount < std::size(out.outputs))
               out.outputs[out.outputCount++] = other;
         } else if (other.direction() == opposite(side) && out.inputCount < std::size(out.inputs)) {
            out.inputs[out.inputCount++] = other;
         }
      }
   };
   add(layout.beltNeighbours);
   if (belt.type() == "splitter") add(layout.splitterNeighbours);
   return out;
}

// The way items come in: a corner's comes from its side.
uint8_t inputDirection(Belt belt) {
   const uint32_t shape = belt.shape();
   if (shape == layout.beltShapeLeft) return clockwise(belt.direction());
   if (shape == layout.beltShapeRight) return counterclockwise(belt.direction());
   return belt.direction();
}

// The belts behind and to each side that feed a transport belt or an underground belt
// (transport-belts.lua's get_parents); an underground exit's behind is its entrance.
struct Parents {
   Belt behind;
   Belt left;
   Belt right;
};

Parents parentsOf(Belt belt, const Neighbours& neighbours) {
   Parents out;
   if (!belt.isTransportBelt() && !belt.isUnderground()) return out;
   const uint8_t outgoing = belt.direction();
   if (neighbours.inputCount == 1 && neighbours.inputs[0].direction() == outgoing) {
      out.behind = neighbours.inputs[0];
   } else {
      const uint8_t incoming = inputDirection(belt);
      for (size_t i = 0; i < neighbours.inputCount; ++i) {
         const Belt input = neighbours.inputs[i];
         if (input.direction() == incoming)
            out.behind = input;
         else if (input.direction() == clockwise(incoming))
            out.left = input;
         else if (input.direction() == counterclockwise(incoming))
            out.right = input;
      }
   }
   if (belt.isUndergroundExit()) out.behind = belt.partner();
   return out;
}

// The words for a transport belt's or an underground belt's shape (scripts/transport-belts.lua,
// Node:get_shape_info, and fa-info.lua's ent_info_belt_shape).
std::vector<std::string> beltShape(Belt belt) {
   const Neighbours neighbours = neighboursOf(belt);
   const uint8_t outgoing = belt.direction();
   const auto [behind, left, right] = parentsOf(belt, neighbours);

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

std::string joined(const std::vector<std::string>& words, std::string_view separator = " ") {
   std::string out;
   for (const std::string& word : words) {
      if (word.empty()) continue;
      if (!out.empty()) out += separator;
      out += word;
   }
   return out;
}

// ---- pipes ----

constexpr int32_t kScale = game::kMapPositionScale;

int32_t tileOf(int32_t coordinate) {
   return coordinate >= 0 ? coordinate / kScale : -((-coordinate + kScale - 1) / kScale);
}

// The entity's fluid boxes, in the order LuaEntity numbers them.
std::vector<const std::byte*> fluidBoxesOf(const std::byte* entity) {
   alignas(16) std::byte set[64];
   if (layout.fluidBoxSetSize > sizeof(set)) return {};
   reinterpret_cast<void* (*)(const void*, void*)>(layout.entityGetFluidBoxes)(entity, set);
   const auto* first = at<const std::byte* const*>(set, layout.fluidBoxSetBegin);
   const auto* last = at<const std::byte* const*>(set, layout.fluidBoxSetEnd);
   std::vector<const std::byte*> boxes(first, last);
   reinterpret_cast<void (*)(void*)>(layout.fluidBoxSetDestroy)(set);
   return boxes;
}

const std::byte* ownerOf(const std::byte* box) { return at<const std::byte*>(box, layout.fluidBoxOwner); }

// Where a fluid box's pipe connection is, and which way it leads out of the entity.
struct Location {
   Position position{};
   uint8_t direction = kNorth;
};

Location locate(const std::byte* box, size_t index) {
   const std::byte* owner = ownerOf(box);
   const std::byte* definition =
      at<const std::byte*>(at<const std::byte*>(box, layout.fluidBoxPrototype), layout.pipeDefinitionsBegin) +
      index * layout.pipeDefinitionSize;
   uint8_t direction = 0;
   virtualAt<void* (*)(const void*, uint8_t*)>(owner, layout.entityGetDirection)(owner, &direction);
   uint8_t mirrored = 0;
   virtualAt<void* (*)(const void*, uint8_t*)>(owner, layout.entityIsMirrored)(owner, &mirrored);
   alignas(8) std::byte out[32]{};
   if (layout.pipeLocationSize > sizeof(out)) return {};
   using LocationFunction =
      void* (*)(const void* definition, void* out, Position position, uint8_t direction, uint8_t mirrored);
   reinterpret_cast<LocationFunction>(layout.pipeLocation)(definition, out, at<Position>(owner, layout.entityPosition),
                                                           direction, mirrored);
   return {at<Position>(out, layout.pipeLocationPosition), at<uint8_t>(out, layout.pipeLocationDirection)};
}

// One pipe connection, as get_fluid_box_pipe_connections gives it.
struct PipeConnection {
   Location location;
   uint8_t flow = 0;                 // FluidFlowDirection
   uint8_t type = 0;                 // PipeConnectionType
   const std::byte* target = nullptr; // the FluidBox it joins
   uint16_t opposite = 0;            // its index among the target's connections
};

std::vector<PipeConnection> connectionsOf(const std::byte* box) {
   const auto* first = at<const std::byte*>(box, layout.fluidBoxConnectionsBegin);
   const auto* last = at<const std::byte*>(box, layout.fluidBoxConnectionsEnd);
   const std::byte* definitions =
      at<const std::byte*>(at<const std::byte*>(box, layout.fluidBoxPrototype), layout.pipeDefinitionsBegin);
   std::vector<PipeConnection> out;
   for (size_t i = 0; first + i * layout.fluidConnectionSize < last; ++i) {
      const std::byte* connection = first + i * layout.fluidConnectionSize;
      const std::byte* definition = definitions + i * layout.pipeDefinitionSize;
      out.push_back({locate(box, i), at<uint8_t>(definition, layout.pipeDefinitionFlow),
                     at<uint8_t>(definition, layout.pipeDefinitionType),
                     at<const std::byte*>(connection, layout.fluidConnectionTarget),
                     at<uint16_t>(connection, layout.fluidConnectionOpposite)});
   }
   return out;
}

bool isPipe(const std::byte* entity) {
   const std::string_view type = typeName(prototypeOf(entity));
   return type == "pipe" || type == "infinity-pipe";
}

// The shape the joined sides of a pipe or heat pipe make (fa-info.lua's pipe_shape), from the
// ways its connections lead; nothing when one leads elsewhere than the four sides.
std::string shapeOf(const bool (&joined)[16]) {
   int count = 0;
   for (bool side : joined) count += side;
   const bool north = joined[kNorth], east = joined[kEast], south = joined[kSouth], west = joined[kWest];
   if (north + east + south + west != count) return {};
   switch (count) {
   case 0: return vocab::kPipeAlone;
   case 1: {
      const uint8_t side = north ? kNorth : east ? kEast : south ? kSouth : kWest;
      // Named by the way it points, away from its one connection.
      return vocab::kPipeEnd(directionName(opposite(side)));
   }
   case 2:
      if (north && south) return vocab::kPipeVertical;
      if (east && west) return vocab::kPipeHorizontal;
      return vocab::kPipeCorner(directionName(north ? kNorth : kSouth), directionName(east ? kEast : kWest));
   case 3: {
      const uint8_t missing = !north ? kNorth : !east ? kEast : !south ? kSouth : kWest;
      const vocab::Word key = missing == kNorth || missing == kSouth ? vocab::kPipeTHorizontal : vocab::kPipeTVertical;
      return key(directionName(opposite(missing)));
   }
   default: return vocab::kPipeCross;
   }
}

// A pipe's shape from the sides it joins to anything, a pipe, a machine or a tank, so a pipe that
// leads into an engine is no lonely pipe.
std::string pipeShape(const std::byte* entity) {
   const std::vector<const std::byte*> boxes = fluidBoxesOf(entity);
   if (boxes.size() != 1) return {};
   bool joined[16] = {};
   for (const PipeConnection& connection : connectionsOf(boxes[0]))
      if (connection.target) joined[connection.location.direction % 16] = true;
   return shapeOf(joined);
}

// What a fluid box must hold (fluids.lua's get_local_fluidbox_constraint): its filter, else its
// segment's fluid; 0 for none.
uint16_t localFluid(const std::byte* box) {
   const std::byte* segment = at<const std::byte*>(box, layout.fluidBoxSegment);
   if (segment && at<uint16_t>(segment, layout.segmentFilter)) return at<uint16_t>(segment, layout.segmentFilter);
   if (const uint16_t filter = at<uint16_t>(box, layout.fluidBoxFilter)) return filter;
   return segment ? at<uint16_t>(segment, layout.segmentFluid) : uint16_t{0};
}

// Its own, else the first a box it joins must hold.
uint16_t requiredFluid(const std::byte* box, const std::vector<PipeConnection>& connections) {
   if (const uint16_t fluid = localFluid(box)) return fluid;
   for (const PipeConnection& connection : connections)
      if (connection.target)
         if (const uint16_t fluid = localFluid(connection.target)) return fluid;
   return 0;
}

const std::byte* prototypeAt(uintptr_t list, uint32_t id) {
   struct Vector {
      const std::byte* const* first;
      const std::byte* const* last;
      const std::byte* const* end;
   };
   const auto& prototypes = *reinterpret_cast<const Vector*>(list);
   return id < static_cast<size_t>(prototypes.last - prototypes.first) ? prototypes.first[id] : nullptr;
}

std::string localisedName(const std::byte* prototype) {
   struct MsvcString {
      union {
         char buffer[16];
         const char* pointer;
      };
      size_t size;
      size_t capacity;
   };
   using Str = const MsvcString* (*)(const void* localisedString, const void* localeProvider);
   const MsvcString* name =
      reinterpret_cast<Str>(layout.localisedStringStr)(prototype + layout.prototypeLocalisedName, nullptr);
   return text::speakable({name->capacity >= 16 ? name->pointer : name->buffer, name->size});
}

// A connection point on the tile, rolled up as fa-info.lua's ent_info_fluid_connections does.
struct Point {
   uint8_t outward = kNorth;
   bool input = false;
   bool output = false;
   uint8_t type = 0;
   bool connected = false;
   bool open = true;
   uint16_t fluid = 0;
   int32_t tiles = 1;
};

// What a fluid point carries, as said: a fluid, any fluid, or closed.
std::string fluidWords(const Point& point) {
   if (!point.open) return vocab::kFluidClosed;
   if (!point.fluid) return vocab::kAnyFluid;
   const std::byte* prototype = prototypeAt(layout.fluidPrototypes, point.fluid);
   return prototype ? localisedName(prototype) : std::string(vocab::kAnyFluid);
}

std::vector<Point> pointsOn(const std::byte* entity, Position tile) {
   std::vector<Point> points;
   if (typeName(prototypeOf(entity)) == "fluid-wagon") return points;
   const bool pipe = isPipe(entity);
   const std::byte* machine = agui::objectAsBase(entity, ".?AVCraftingMachine@@");
   const bool noRecipe = machine && !at<uint16_t>(machine + layout.craftingRecipe, layout.idWithQualityBase);
   for (const std::byte* box : fluidBoxesOf(entity)) {
      const std::vector<PipeConnection> connections = connectionsOf(box);
      const uint16_t fluid = requiredFluid(box, connections);
      for (const PipeConnection& connection : connections) {
         const Position where = connection.location.position;
         if (tileOf(where.x) != tile.x || tileOf(where.y) != tile.y) continue;
         // A pipe's shape says where it joins; a line of its own is left for what it joins that is
         // no pipe, to name the fluid.
         if (pipe && (!connection.target || isPipe(ownerOf(connection.target)))) continue;
         Point point;
         point.outward = connection.location.direction % 16;
         point.input = connection.flow != layout.flowOutput;
         point.output = connection.flow != layout.flowInput;
         point.type = connection.type;
         point.connected = connection.target != nullptr;
         point.open = !noRecipe || at<uint16_t>(box, layout.fluidBoxFilter) != 0;
         point.fluid = fluid;
         if (connection.type != layout.pipeTypeNormal && connection.target) {
            const Position other = locate(connection.target, connection.opposite).position;
            const double dx = static_cast<double>(other.x - where.x) / kScale;
            const double dy = static_cast<double>(other.y - where.y) / kScale;
            point.tiles = static_cast<int32_t>(std::ceil(std::sqrt(dx * dx + dy * dy)));
         }
         points.push_back(point);
      }
   }
   return points;
}

// The tile's fluid connections, a list item per fluid, distance and way: in, out, then both ways.
std::vector<std::string> fluidConnections(const std::byte* entity, Position tile) {
   // By the way each leads out: what comes in, what goes out, and what does both.
   const Point* in[16] = {};
   const Point* out[16] = {};
   const Point* both[16] = {};
   const std::vector<Point> points = pointsOn(entity, tile);
   for (const Point& point : points) {
      if (point.input) in[point.outward] = &point;
      if (point.output) out[point.outward] = &point;
   }
   for (size_t way = 0; way < 16; ++way)
      if (in[way] && out[way]) {
         both[way] = in[way];
         in[way] = out[way] = nullptr;
      }

   std::vector<std::string> items;
   auto present = [&](vocab::Word key, const Point* const(&set)[16]) {
      // Grouped by what they carry and how far they reach, in the order first met.
      std::vector<std::pair<std::string, int32_t>> groups;
      for (const Point* point : set)
         if (point) {
            std::pair<std::string, int32_t> group{fluidWords(*point), point->tiles};
            if (std::find(groups.begin(), groups.end(), group) == groups.end()) groups.push_back(std::move(group));
         }
      for (const auto& [fluid, tiles] : groups) {
         std::vector<std::string> ways;
         for (size_t way = 0; way < 16; ++way) {
            const Point* point = set[way];
            if (!point || fluidWords(*point) != fluid || point->tiles != tiles) continue;
            const std::string name = directionName(static_cast<uint8_t>(way));
            if (point->connected)
               ways.push_back(name);
            else if (point->type == layout.pipeTypeUnderground)
               ways.push_back(vocab::kUndergroundUnconnected(name));
            else if (point->type == layout.pipeTypeNormal)
               ways.push_back(vocab::kNotConnected(name));
         }
         items.push_back(key(fluid, joined(ways, ", "), tiles));
      }
   };
   present(vocab::kFluidIn, in);
   present(vocab::kFluidOut, out);
   present(vocab::kFluidBoth, both);
   return items;
}

// ---- heat ----

// A heat connection: the tile it is on, the way it leads out, and the entity it joins (null while
// it joins none).
struct HeatPort {
   Position tile{};
   uint8_t direction = kNorth;
   const std::byte* joined = nullptr;
};

// The entity's heat connections, where the game itself finds them (HeatBuffer::findOtherEnd).
std::vector<HeatPort> heatPortsOf(const std::byte* entity) {
   std::vector<HeatPort> ports;
   if (typeName(prototypeOf(entity)) == "entity-ghost") return ports;
   const std::byte* buffer = virtualAt<const std::byte* (*)(const void*)>(entity, layout.entityGetHeatBuffer)(entity);
   if (!buffer) return ports;
   uint8_t direction = 0;
   virtualAt<void* (*)(const void*, uint8_t*)>(entity, layout.entityGetDirection)(entity, &direction);
   uint8_t mirrored = 0;
   virtualAt<void* (*)(const void*, uint8_t*)>(entity, layout.entityIsMirrored)(entity, &mirrored);
   const Position position = at<Position>(entity, layout.entityPosition);
   const std::byte* definitions =
      at<const std::byte*>(at<const std::byte*>(buffer, layout.heatBufferPrototype), layout.heatDefinitionsBegin);
   const auto* first = at<const std::byte*>(buffer, layout.heatConnectionsBegin);
   const auto* last = at<const std::byte*>(buffer, layout.heatConnectionsEnd);
   for (size_t i = 0; first + i * layout.heatConnectionSize < last; ++i) {
      const std::byte* definition = definitions + i * layout.heatDefinitionSize;
      // A Vector2<double>: x, then y.
      alignas(8) double offset[2] = {at<double>(definition, layout.heatDefinitionX),
                                     at<double>(definition, layout.heatDefinitionY)};
      auto way = at<uint8_t>(definition, layout.heatDefinitionDirection);
      if (mirrored) {
         offset[0] = -offset[0];
         way = static_cast<uint8_t>(-way);
      }
      reinterpret_cast<void (*)(double*, uint8_t)>(layout.vectorRotate)(offset, direction);
      HeatPort port;
      port.tile = {(position.x - static_cast<int32_t>(offset[0] * -kScale)) >> 8,
                   (position.y - static_cast<int32_t>(offset[1] * -kScale)) >> 8};
      port.direction = static_cast<uint8_t>((way + direction) & 15);
      if (const std::byte* target =
             at<const std::byte*>(first + i * layout.heatConnectionSize, layout.heatConnectionTarget))
         if (const std::byte* holder = at<const std::byte*>(target, layout.heatBufferHolder))
            port.joined = at<const std::byte*>(holder, layout.heatHolderOwner);
      ports.push_back(port);
   }
   return ports;
}

bool isHeatPipe(const std::byte* entity) { return typeName(prototypeOf(entity)) == "heat-pipe"; }

// A nuclear reactor's state (fa-info.lua's ent_info_nuclear_neighbor_bonus), each a list item: too
// hot, burning a fuel cell, and its neighbour bonus.
std::vector<std::string> reactorState(const std::byte* entity) {
   std::vector<std::string> out;
   if (internalName(prototypeOf(entity)) != "nuclear-reactor") return out;
   if (const std::byte* buffer =
          virtualAt<const std::byte* (*)(const void*)>(entity, layout.entityGetHeatBuffer)(entity)) {
      const std::byte* prototype = at<const std::byte*>(buffer, layout.heatBufferPrototype);
      const double temperature =
         at<double>(buffer, layout.heatBufferEnergy) / at<double>(prototype, layout.heatSpecificHeat) +
         at<double>(prototype, layout.heatDefaultTemperature);
      if (temperature > 900) out.emplace_back(vocab::kReactorDanger);
   }
   if (virtualAt<double (*)(const void*)>(entity, layout.entityAvailableEnergy)(entity) > 0)
      out.emplace_back(vocab::kReactorConsuming);
   const double bonus = reinterpret_cast<double (*)(const void*)>(layout.reactorNeighbourBonus)(entity);
   out.push_back(vocab::kReactorBonus(static_cast<int64_t>(std::floor(bonus * 100))));
   return out;
}

// A heat pipe's shape from the sides it joins to anything, as a pipe's.
std::string heatPipeShape(const std::byte* entity) {
   bool joined[16] = {};
   for (const HeatPort& port : heatPortsOf(entity))
      if (port.joined) joined[port.direction] = true;
   return shapeOf(joined);
}

// The heat connections on the tile (fa-info.lua's ent_info_heat_neighbors): which ways they lead,
// unless it is a heat pipe, whose shape says so, and what they join, but for a heat pipe's other
// heat pipes.
std::string heatConnections(const std::byte* entity, Position tile) {
   const bool pipe = isHeatPipe(entity);
   std::vector<std::string> ways;
   // Each joined entity's name, then the way to it.
   std::vector<std::string> partners;
   for (const HeatPort& port : heatPortsOf(entity)) {
      if (port.tile.x != tile.x || port.tile.y != tile.y) continue;
      ways.push_back(directionName(port.direction));
      if (port.joined && !(pipe && isHeatPipe(port.joined))) {
         partners.push_back(localisedName(prototypeOf(port.joined)));
         partners.push_back(directionName(port.direction));
      }
   }
   std::vector<std::string> words;
   if (!pipe) {
      const auto& w = ways;
      switch (ways.size()) {
      case 1: words.push_back(vocab::kHeatPorts[0](w[0])); break;
      case 2: words.push_back(vocab::kHeatPorts[1](w[0], w[1])); break;
      case 3: words.push_back(vocab::kHeatPorts[2](w[0], w[1], w[2])); break;
      case 4: words.push_back(vocab::kHeatPorts[3](w[0], w[1], w[2], w[3])); break;
      default: break;
      }
   }
   const auto& j = partners;
   switch (partners.size() / 2) {
   case 0: break;
   case 1: words.push_back(vocab::kHeatConnected[0](j[0], j[1])); break;
   case 2: words.push_back(vocab::kHeatConnected[1](j[0], j[1], j[2], j[3])); break;
   case 3: words.push_back(vocab::kHeatConnected[2](j[0], j[1], j[2], j[3], j[4], j[5])); break;
   default: words.push_back(vocab::kHeatConnected[3](j[0], j[1], j[2], j[3], j[4], j[5], j[6], j[7])); break;
   }
   return joined(words);
}

// ---- what a belt carries ----

// An item on a lane: its item and quality IDs and how many are stacked.
struct LaneItem {
   uint16_t item;
   uint8_t quality;
   uint32_t count;
};

// A lane line's items front to back, each with its index and position (fixed point, 0 at the
// line's front, growing upstream), until `visit` returns false.
template <class Visit>
void eachItem(const std::byte* line, Visit&& visit) {
   const std::byte* data = at<const std::byte*>(line, layout.lineItems);
   const uint32_t capacity = at<uint32_t>(line, layout.lineItemsCapacity);
   const uint32_t head = at<uint32_t>(line, layout.lineItemsHead);
   const uint32_t size = at<uint32_t>(line, layout.lineItemsSize);
   int32_t position = at<int32_t>(line, layout.lineFirst);
   for (uint32_t i = 0; i < size; ++i) {
      uint32_t slot = head + i;
      if (slot >= capacity) slot -= capacity;
      const std::byte* item = data + static_cast<size_t>(slot) * layout.lineItemSize;
      if (!visit(i, position, item)) return;
      position += at<int32_t>(item, layout.lineItemGap);
   }
}

LaneItem laneItem(const std::byte* item) {
   const std::byte* stack = item + layout.lineItemStack;
   return {at<uint16_t>(stack, layout.itemStackItem), at<uint8_t>(stack, layout.itemStackQuality),
           at<uint32_t>(stack, layout.itemStackCount)};
}

// Lane 1 (left) or 2 (right) of a belt connectable: its lane line and the range [start, end) of it
// on this entity.
struct LaneRange {
   const std::byte* line = nullptr;
   int32_t start = 0;
   int32_t end = 0;
};

LaneRange rangeOf(Belt belt, int index) {
   const std::byte* connector = belt.entity + (index == 1 ? layout.beltLeftLine : layout.beltRightLine);
   return {at<const std::byte*>(connector, layout.connectorLine), at<int32_t>(connector, layout.connectorStart),
           at<int32_t>(connector, layout.connectorEnd)};
}

// TransportLine::firstGapIndex when no item has room behind it.
constexpr uint32_t kNoGap = 0xffffffff;

// The items on a belt's lane; whether they fill it, four a tile, as LuaTransportLine counts a full
// lane; and whether they stand still. TransportLine::move moves a line's items while its front
// item has room to go on; when it has none (it is at the front, distanceToFirst 0) it only closes
// the first gap, so the items from the front up to the first one with room behind it
// (firstGapIndex) stay where they are, and with no gap none move.
struct Lane {
   std::vector<LaneItem> items;
   bool full = false;
   bool stopped = false;
};

Lane laneOf(LaneRange range) {
   Lane lane;
   if (!range.line) return lane;
   uint32_t last = 0;
   eachItem(range.line, [&](uint32_t index, int32_t position, const std::byte* item) {
      if (position >= range.end) return false;
      if (position >= range.start) {
         lane.items.push_back(laneItem(item));
         last = index;
      }
      return true;
   });
   lane.full = static_cast<int64_t>(lane.items.size()) * kScale == static_cast<int64_t>(range.end - range.start) * 4;
   const uint32_t gap = at<uint32_t>(range.line, layout.lineGapIndex);
   lane.stopped =
      !lane.items.empty() && at<int32_t>(range.line, layout.lineFirst) == 0 && (gap == kNoGap || last <= gap);
   return lane;
}

// Items found off a belt: those on the nearest tile with any, and how many tiles away it is,
// negative behind.
struct Carried {
   std::vector<LaneItem> items;
   int distance = 0;
};

// The most lane lines a search follows; a loop of belts comes back to one it has seen first.
constexpr size_t kMaxLines = 1000;

// Behind a belt: further back along its lane line, then along each line feeding the back of the
// one before (TransportLine::directInput), as the items came.
Carried behind(LaneRange range) {
   Carried out;
   const std::byte* line = range.line;
   int32_t from = range.end;
   int64_t offset = 0;
   std::vector<const std::byte*> seen;
   while (line && seen.size() < kMaxLines && std::find(seen.begin(), seen.end(), line) == seen.end()) {
      seen.push_back(line);
      int64_t tile = -1;
      eachItem(line, [&](uint32_t, int32_t position, const std::byte* item) {
         if (position < from) return true;
         const int64_t here = (position - from + offset) / kScale;
         if (tile < 0) tile = here;
         if (here != tile) return false;
         out.items.push_back(laneItem(item));
         return true;
      });
      if (!out.items.empty()) {
         out.distance = -static_cast<int>(tile + 1);
         return out;
      }
      offset += at<int32_t>(line, layout.lineLength) - from;
      from = 0;
      line = at<const std::byte*>(line, layout.lineDirectInput);
   }
   return out;
}

// Ahead of a belt: further on along its lane line, then along the line each one feeds
// (TransportLine::directConnection), from where it joins it: its back, or the place it pours onto
// from the side (directConnectionPosition), as the items will go.
Carried ahead(LaneRange range) {
   Carried out;
   const std::byte* line = range.line;
   int32_t to = range.start;
   int64_t offset = 0;
   std::vector<const std::byte*> seen;
   while (line && seen.size() < kMaxLines && std::find(seen.begin(), seen.end(), line) == seen.end()) {
      seen.push_back(line);
      // Front to back, so the nearest tile comes last.
      int64_t tile = -1;
      std::vector<LaneItem> nearest;
      eachItem(line, [&](uint32_t, int32_t position, const std::byte* item) {
         if (position >= to) return false;
         const int64_t here = (to - 1 - position + offset) / kScale;
         if (here != tile) {
            tile = here;
            nearest.clear();
         }
         nearest.push_back(laneItem(item));
         return true;
      });
      if (!nearest.empty()) {
         out.items = std::move(nearest);
         out.distance = static_cast<int>(tile + 1);
         return out;
      }
      const std::byte* next = at<const std::byte*>(line, layout.lineDirectConnection);
      if (!next) break;
      const int32_t joins = at<int32_t>(line, layout.lineDirectPosition);
      offset += to;
      to = joins != 0 ? joins : at<int32_t>(next, layout.lineLength);
      line = next;
   }
   return out;
}

// What one lane carries (Node:carries_heuristic, without its five-belt limit): the items on this
// belt's lane, else the nearest behind, else the nearest ahead.
Carried carried(const Lane& lane, LaneRange range) {
   if (!lane.items.empty()) return {lane.items, 0};
   if (!range.line) return {};
   if (Carried found = behind(range); !found.items.empty()) return found;
   return ahead(range);
}

// One lane's words for fa.ent-info-belt-contents-*: the most common item (highest quality, then
// name, on a tie), how far it was found, 0 behind, 1 here or 2 ahead, how many other items, this
// belt's lane (1 full, 2 stopped, 3 backed up: full and stopped), and the other item when there is
// just one.
struct LaneWords {
   uint16_t item = 0;
   std::string name;
   int distance = 0;
   int way = 1;
   size_t others = 0;
   int state = 0;
   uint16_t second = 0;
   std::string secondName;

   bool operator==(const LaneWords& other) const {
      return item == other.item && distance == other.distance && way == other.way && others == other.others &&
             state == other.state && second == other.second;
   }
};

LaneWords laneWords(const Carried& lane, const Lane& here) {
   struct Count {
      uint16_t item;
      uint8_t quality;
      uint64_t count;
   };
   std::vector<Count> counts;
   std::vector<uint16_t> items;
   for (const LaneItem& entry : lane.items) {
      auto same = [&](const Count& c) { return c.item == entry.item && c.quality == entry.quality; };
      if (auto it = std::find_if(counts.begin(), counts.end(), same); it != counts.end())
         it->count += entry.count;
      else
         counts.push_back({entry.item, entry.quality, entry.count});
      if (std::find(items.begin(), items.end(), entry.item) == items.end()) items.push_back(entry.item);
   }
   auto level = [](uint8_t quality) {
      const std::byte* prototype = prototypeAt(layout.qualityPrototypes, quality);
      return prototype ? at<uint32_t>(prototype, layout.qualityLevel) : 0u;
   };
   auto itemName = [](uint16_t item) {
      const std::byte* prototype = prototypeAt(layout.itemPrototypes, item);
      return prototype ? internalName(prototype) : std::string_view{};
   };
   const Count* best = nullptr;
   for (const Count& c : counts) {
      if (!best || c.count > best->count ||
          (c.count == best->count &&
           (level(c.quality) > level(best->quality) ||
            (level(c.quality) == level(best->quality) && itemName(c.item) < itemName(best->item)))))
         best = &c;
   }
   auto nameOf = [](uint16_t item) {
      const std::byte* prototype = prototypeAt(layout.itemPrototypes, item);
      return prototype ? localisedName(prototype) : std::string();
   };
   LaneWords out;
   out.item = best->item;
   out.name = nameOf(best->item);
   out.distance = std::abs(lane.distance);
   out.way = lane.distance < 0 ? 0 : lane.distance > 0 ? 2 : 1;
   out.others = items.size() - 1;
   out.state = (here.full ? 1 : 0) + (here.stopped ? 2 : 0);
   if (items.size() == 2) {
      out.second = items[0] == best->item ? items[1] : items[0];
      out.secondName = nameOf(out.second);
   }
   return out;
}

// What a transport belt carries on each lane (fa-info.lua's ent_info_belt_contents).
std::string beltContents(Belt belt) {
   const LaneRange leftRange = rangeOf(belt, 1);
   const LaneRange rightRange = rangeOf(belt, 2);
   const Lane leftLane = laneOf(leftRange);
   const Lane rightLane = laneOf(rightRange);
   const Carried left = carried(leftLane, leftRange);
   const Carried right = carried(rightLane, rightRange);
   if (left.items.empty() && right.items.empty()) return vocab::kBeltCarriesNothing;
   if (right.items.empty()) {
      const LaneWords l = laneWords(left, leftLane);
      return vocab::kBeltCarriesLeft(l.name, l.distance, l.way, l.others, l.state, l.secondName);
   }
   if (left.items.empty()) {
      const LaneWords r = laneWords(right, rightLane);
      return vocab::kBeltCarriesRight(r.name, r.distance, r.way, r.others, r.state, r.secondName);
   }
   const LaneWords l = laneWords(left, leftLane);
   const LaneWords r = laneWords(right, rightLane);
   if (l == r) return vocab::kBeltCarriesBothSame(l.name, l.distance, l.way, l.others, l.state, l.secondName);
   return vocab::kBeltCarriesBoth(l.name, l.distance, l.way, l.others, l.state, l.secondName, r.name, r.distance, r.way,
                                  r.others, r.state, r.secondName);
}

// A splitter's input and output priorities and its filter (transport-belts.lua's
// splitter_priority_info), each side named with the way it leads.
std::string splitterState(Belt splitter) {
   const std::byte* logic = splitter.entity + layout.splitterLogic;
   const uint8_t direction = splitter.direction();
   auto isRight = [&](uint32_t offset) { return at<uint32_t>(logic, offset) == layout.splitterRight; };
   auto way = [&](uint32_t offset) {
      return directionName(isRight(offset) ? clockwise(direction) : counterclockwise(direction));
   };
   std::vector<std::string> words;
   if (!at<bool>(logic, layout.splitterInputLocked))
      words.emplace_back(vocab::kSplitterInputBalanced);
   else if (isRight(layout.splitterTakeFrom))
      words.push_back(vocab::kSplitterInputRight(way(layout.splitterTakeFrom)));
   else
      words.push_back(vocab::kSplitterInputLeft(way(layout.splitterTakeFrom)));

   const uint16_t filter = at<uint16_t>(logic + layout.splitterFilter, layout.itemFilterId);
   const std::byte* filtered = filter ? prototypeAt(layout.itemPrototypes, filter) : nullptr;
   if (filtered) {
      const std::string side = isRight(layout.splitterGoesTo) ? vocab::kRight : vocab::kLeft;
      words.push_back(vocab::kSplitterFiltering(localisedName(filtered), side, way(layout.splitterGoesTo)));
   } else if (!at<bool>(logic, layout.splitterOutputLocked)) {
      words.emplace_back(vocab::kSplitterOutputBalanced);
   } else if (isRight(layout.splitterGoesTo)) {
      words.push_back(vocab::kSplitterOutputRight(way(layout.splitterGoesTo)));
   } else {
      words.push_back(vocab::kSplitterOutputLeft(way(layout.splitterGoesTo)));
   }
   return joined(words);
}

// ---- power poles ----

bool isPole(const std::byte* entity) { return typeName(prototypeOf(entity)) == "electric-pole"; }

// How many copper wires a pole has (fa-info.lua's ent_info_pole_neighbors).
std::string poleWires(const std::byte* pole) {
   const size_t count = (at<uintptr_t>(pole, layout.poleWiresEnd) - at<uintptr_t>(pole, layout.poleWiresBegin)) /
                        layout.wireConnectionSize;
   if (count == 0) return vocab::kPoleNoWires;
   return vocab::kPoleWires(static_cast<int64_t>(count));
}

// How many buildings a pole powers and how many feed it (fa-info.lua's ent_info_electric_pole):
// those the game joins to its network, with an electric energy source in its supply area.
std::string poleSupply(const std::byte* pole) {
   using SupplyArea = void* (*)(const void* pole, std::byte* out);
   using Search = void* (*)(std::byte* search, const void* surface, const std::byte* area);
   using Destroy = void (*)(std::byte* search);
   using ElectricSource = const void* (*)(const void* entity);
   using Quality = uint8_t* (*)(const void* entity, uint8_t* out);
   using Consumption = double (*)(const void* prototype, uint8_t quality, const void* force);
   using Output = double (*)(const void* prototype, uint8_t quality);

   alignas(8) std::byte area[game::kBoundingBoxCapacity]{};
   reinterpret_cast<SupplyArea>(layout.poleSupplyArea)(pole, area);
   alignas(8) std::byte search[game::kEntitySearchCapacity]{};
   reinterpret_cast<Search>(layout.entitySearch)(search, at<const void*>(pole, layout.entitySurface), area);
   const auto* first = at<const std::byte* const*>(search, layout.entitySearchContent);
   const auto* last = at<const std::byte* const*>(search, layout.entitySearchContent + sizeof(void*));
   int64_t supplied = 0;
   int64_t producers = 0;
   for (const std::byte* const* it = first; it != last; ++it) {
      const std::byte* entity = *it;
      if (!reinterpret_cast<ElectricSource>(layout.entityElectricSource)(entity)) continue;
      uint8_t quality = 0;
      virtualAt<Quality>(entity, layout.entityGetQuality)(entity, &quality);
      const std::byte* prototype = prototypeOf(entity);
      if (virtualAt<Consumption>(prototype, layout.prototypePowerConsumption)(prototype, quality, nullptr) > 0)
         ++supplied;
      else if (virtualAt<Output>(prototype, layout.prototypePowerOutput)(prototype, quality) > 0)
         ++producers;
   }
   reinterpret_cast<Destroy>(layout.entitySearchDestroy)(search);

   std::vector<std::string> words{vocab::kPoleSupplying(supplied)};
   if (producers > 0) words.push_back(vocab::kPoleDrawingFrom(producers));
   return joined(words);
}

// ---- mining drills ----

struct Box {
   Position leftTop;
   Position rightBottom;
};

Box selectionBoxOf(const std::byte* entity) {
   using SelectionBox = void* (*)(const void* entity, std::byte* out, const void* context);
   // A BoundingBox, and a SelectionContext the call ignores.
   alignas(8) std::byte out[game::kBoundingBoxCapacity]{};
   alignas(8) std::byte context[64]{};
   reinterpret_cast<SelectionBox>(layout.entitySelectionBox)(entity, out, context);
   return at<Box>(out, 0);
}

// Whether the tile is the drill's own tile its output comes out of, the one nearest where it drops
// (ResourceMining.get_solid_output_coords). A pumpjack drops at its own position: it has none.
bool isDrillOutput(const std::byte* drill, Position tile) {
   using DropPosition = const Position* (*)(const void* entity, Position* out);
   Position drop{};
   virtualAt<DropPosition>(drill, layout.entityDropPosition)(drill, &drop);
   const Position position = at<Position>(drill, layout.entityPosition);
   if (drop.x == position.x && drop.y == position.y) return false;
   const Box box = selectionBoxOf(drill);
   const int32_t centreX = tileOf(drop.x) * kScale + kScale / 2;
   const int32_t centreY = tileOf(drop.y) * kScale + kScale / 2;
   return tileOf(std::clamp(centreX, box.leftTop.x, box.rightBottom.x - 1)) == tile.x &&
          tileOf(std::clamp(centreY, box.leftTop.y, box.rightBottom.y - 1)) == tile.y;
}

// ---- infinity chests and pipes ----

// The first three filters, each an item and how many it keeps, the rest counted, and whether
// the chest trashes the rest (fa-info.lua's ent_info_infinity_chest), as list items.
std::vector<std::string> infinityChestState(const std::byte* chest) {
   const auto* first = at<const std::byte*>(chest, layout.infinityChestFilters);
   const auto* last = at<const std::byte*>(chest, layout.infinityChestFilters + sizeof(void*));
   const size_t count = static_cast<size_t>(last - first) / layout.infinityFilterSize;
   std::vector<std::string> items;
   if (count > 0) items.emplace_back(vocab::kInfinityFilters);
   for (size_t i = 0; i < count && i < 3; ++i) {
      const std::byte* filter = first + i * layout.infinityFilterSize;
      const std::byte* item = prototypeAt(layout.itemPrototypes, at<uint16_t>(filter, layout.infinityFilterItem));
      const uint32_t keep = at<uint32_t>(filter, layout.infinityFilterCount);
      const uint8_t mode = at<uint8_t>(filter, layout.infinityFilterMode);
      const vocab::Word& how = mode == layout.infinityModeAtLeast  ? vocab::kInfinityAtLeast
                               : mode == layout.infinityModeAtMost ? vocab::kInfinityAtMost
                                                                   : vocab::kInfinityExactly;
      items.push_back(joined({item ? localisedName(item) : std::string(), how(keep)}));
   }
   if (count > 3) items.back() = joined({items.back(), vocab::kAndOthers(count - 3)});
   if (at<bool>(chest, layout.infinityChestRemoveUnfiltered)) {
      if (items.empty())
         items.emplace_back(vocab::kInfinityTrashUnrequested);
      else
         items.back() = joined({items.back(), vocab::kInfinityTrashUnrequested});
   }
   return items;
}

// ---- logistic networks ----

// FaUtils.get_direction_biased: the 8-way direction from `origin` to `target` by whole tiles,
// keeping to a cardinal unless the diagonal is clear.
uint8_t directionBiased(Position target, Position origin) {
   const int32_t dx = tileOf(target.x) - tileOf(origin.x);
   const int32_t dy = tileOf(target.y) - tileOf(origin.y);
   if (std::abs(dx) > 4 * std::abs(dy)) return dx > 0 ? kEast : kWest;
   if (std::abs(dy) > 4 * std::abs(dx)) return dy > 0 ? kSouth : kNorth;
   if (dx > 0 && dy > 0) return turned(kSouth, -1);
   if (dx > 0 && dy < 0) return turned(kNorth, 1);
   if (dx < 0 && dy > 0) return turned(kSouth, 1);
   if (dx < 0 && dy < 0) return turned(kNorth, -1);
   return kNorth;
}

// The custom name, else the number, as the mod names a network.
std::string networkName(const std::byte* network) {
   struct MsvcString {
      union {
         char buffer[16];
         const char* pointer;
      };
      size_t size;
      size_t capacity;
   };
   const auto& name = *reinterpret_cast<const MsvcString*>(network + layout.logisticNetworkName);
   const std::string_view custom{name.capacity >= sizeof(name.buffer) ? name.pointer : name.buffer, name.size};
   if (!custom.empty()) return text::speakable(custom);
   return std::to_string(at<uint32_t>(network, layout.logisticNetworkId));
}

// The network a logistic chest is in, or how far and which way the nearest network's logistic area
// is (fa-info.lua's ent_info_logistic_network).
std::string logisticNetwork(const std::byte* chest) {
   using FindNetwork = const std::byte* (*)(const void* manager, const Position* position);
   using FindCell = const std::byte* (*)(const void* network, const Position* position);
   using ForceId = uint8_t* (*)(const void* entity, uint8_t* out);

   const std::byte* surface = at<const std::byte*>(chest, layout.entitySurface);
   const std::byte* map = at<const std::byte*>(surface, layout.surfaceMap);
   uint8_t forceId = 0;
   virtualAt<ForceId>(chest, layout.entityGetForceId)(chest, &forceId);
   const std::byte* force = at<const std::byte* const*>(map, layout.mapForceData)[forceId];
   const auto* managers = at<const std::byte* const*>(force, layout.forceLogisticManagers);
   const auto* managersEnd = at<const std::byte* const*>(force, layout.forceLogisticManagers + sizeof(void*));
   const uint32_t index = at<uint32_t>(surface, layout.surfaceIndex);
   const std::byte* manager = index < static_cast<size_t>(managersEnd - managers) ? managers[index] : nullptr;
   if (!manager) return vocab::kLogisticNoNetwork;

   const Position position = at<Position>(chest, layout.entityPosition);
   if (const std::byte* network = reinterpret_cast<FindNetwork>(layout.findMatchingNetworkConst)(manager, &position))
      return vocab::kLogisticInNetwork(networkName(network));
   const std::byte* nearest = reinterpret_cast<FindNetwork>(layout.findClosestNetwork)(manager, &position);
   if (!nearest) return vocab::kLogisticNoNetwork;
   const std::byte* cell = reinterpret_cast<FindCell>(layout.findCellClosestTo)(nearest, &position);
   if (!cell) return vocab::kLogisticNoNetwork;

   const Position centre = at<Position>(at<const std::byte*>(cell, layout.cellOwner), layout.entityPosition);
   const double radius = at<float>(at<const std::byte*>(cell, layout.cellParameters), layout.cellLogisticRadius);
   auto outside = [&](int32_t point, int32_t middle) {
      const double offset = std::abs(static_cast<double>(point - middle) / kScale);
      return std::max(0.0, offset - radius);
   };
   const double dx = outside(position.x, centre.x);
   const double dy = outside(position.y, centre.y);
   const auto tiles = static_cast<int64_t>(std::ceil(std::sqrt(dx * dx + dy * dy)));
   return vocab::kLogisticNearest(
      vocab::kDistanceTilesDirection(tiles, directionName(directionBiased(centre, position))));
}

// ---- inserters and drills ----

const std::byte* dropTargetOf(const std::byte* entity) {
   return virtualAt<const std::byte* (*)(const void*)>(entity, layout.entityDropTarget)(entity);
}

// What is at the end of an inserter's hand or a drill's chute, by its name, or the ground.
std::string targetName(const std::byte* target) {
   return target ? localisedName(prototypeOf(target)) : std::string(vocab::kGround);
}

// The way a hand reaches, from its vector in tiles, rounded as FaUtils.get_direction_biased does.
uint8_t wayOf(double dx, double dy) {
   const double ax = std::abs(dx);
   const double ay = std::abs(dy);
   if (ax > 4 * ay) return dx > 0 ? kEast : kWest;
   if (ay > 4 * ax) return dy > 0 ? kSouth : kNorth;
   if (dx > 0) return dy > 0 ? turned(kSouth, -1) : turned(kNorth, 1);
   return dy > 0 ? turned(kSouth, 1) : turned(kNorth, -1);
}

// What an inserter holds, and where it picks up and drops: the entity there or the ground, how
// many tiles and which way (fa-info.lua's inserter part).
std::string inserterState(const std::byte* inserter) {
   std::vector<std::string> words;
   const std::byte* held = inserter + layout.inserterHeldStack;
   const uint32_t count = at<uint32_t>(held, layout.itemStackCount);
   if (count > 0) {
      const std::byte* item = prototypeAt(layout.itemPrototypes, at<uint16_t>(held, layout.itemStackItem));
      words.push_back(vocab::kInserterHolding(item ? localisedName(item) : std::string()));
      if (count > 1) words.push_back(vocab::kInserterHoldingCount(count));
   }
   auto reach = [&](const vocab::Word& word, const std::byte* target, uint32_t vector) {
      const double dx = at<double>(inserter, vector);
      const double dy = at<double>(inserter, vector + static_cast<uint32_t>(sizeof(double)));
      const auto tiles = static_cast<int64_t>(std::lround(std::sqrt(dx * dx + dy * dy)));
      words.push_back(word(targetName(target), tiles, directionName(wayOf(dx, dy))));
   };
   reach(vocab::kInserterPicksUp, at<const std::byte*>(inserter, layout.inserterPickupTarget),
         layout.inserterPickupVector);
   reach(vocab::kInserterDrops, dropTargetOf(inserter), layout.inserterDropVector);
   return joined(words);
}

// ---- turrets, combinators, signals and switches ----

// The targets a turret shoots first or only (fa-info.lua's ent_info_turret_priority), in FA's
// words, which are shorter than the tooltip's.
std::string turretPriorities(const std::byte* turret) {
   const auto* first = at<const uint16_t*>(turret, layout.turretPriorityList);
   const auto* last = at<const uint16_t*>(turret, layout.turretPriorityList + sizeof(void*));
   std::vector<std::string> names;
   for (const uint16_t* id = first; id != last; ++id)
      if (const std::byte* prototype = *id ? prototypeAt(layout.entityPrototypes, *id) : nullptr)
         names.push_back(localisedName(prototype));
   if (names.empty()) return {};
   const vocab::Word& how =
      at<bool>(turret, layout.turretIgnoreOthers) ? vocab::kTurretOnlyShoots : vocab::kTurretPrioritizes;
   return joined({how, joined(names, ", ")});
}

// A combinator's description, as its player wrote it.
std::string combinatorDescription(const std::byte* entity) {
   struct MsvcString {
      union {
         char buffer[16];
         const char* pointer;
      };
      size_t size;
      size_t capacity;
   };
   const std::byte* combinator = agui::objectAsBase(entity, ".?AVCombinator@@");
   const std::byte* constant = combinator ? nullptr : agui::objectAsBase(entity, ".?AVConstantCombinator@@");
   if (!combinator && !constant) return {};
   const auto& text = combinator
                         ? *reinterpret_cast<const MsvcString*>(combinator + layout.combinatorDescription)
                         : *reinterpret_cast<const MsvcString*>(constant + layout.constantCombinatorDescription);
   return text::speakable({text.capacity >= sizeof(text.buffer) ? text.pointer : text.buffer, text.size});
}

std::string signalState(const std::byte* entity, std::string_view type) {
   if (type == "rail-signal") {
      const auto state = static_cast<uint32_t>(at<int32_t>(entity, layout.signalState));
      if (state == layout.signalOpen) return vocab::kSignalOpen;
      if (state == layout.signalClosed) return vocab::kSignalClosed;
      if (state == layout.signalReserved) return vocab::kSignalReserved;
      if (state == layout.signalReservedByCircuit) return vocab::kSignalReservedByCircuit;
   } else if (type == "rail-chain-signal") {
      const auto state = static_cast<uint32_t>(at<int32_t>(entity, layout.chainSignalState));
      if (state == layout.chainAllOpen) return vocab::kChainAllOpen;
      if (state == layout.chainNone) return vocab::kChainNone;
      if (state == layout.chainNoneOpen) return vocab::kChainNoneOpen;
      if (state == layout.chainPartiallyOpen) return vocab::kChainPartiallyOpen;
   }
   return {};
}

const std::byte* wireConnector(const std::byte* entity, uint32_t id) {
   using Connector = const std::byte* (*)(const void* entity, uint8_t id, bool create);
   return virtualAt<Connector>(entity, layout.entityWireConnector)(entity, static_cast<uint8_t>(id), false);
}

// The circuit network on a connector, or null.
const std::byte* circuitNetwork(const std::byte* entity, uint32_t id) {
   const std::byte* connector = wireConnector(entity, id);
   const std::byte* circuit = connector ? agui::objectAsBase(connector, ".?AVCircuitWireConnector@@") : nullptr;
   return circuit ? at<const std::byte*>(circuit, layout.circuitConnectorNetwork) : nullptr;
}

uint32_t networkId(const std::byte* network) { return at<uint32_t>(network, layout.circuitNetworkIndex); }

// The electric network a power switch side is wired to, by number, or 0 (LuaWireConnector::
// network_id, when it has wires).
uint32_t copperNetwork(const std::byte* entity, uint32_t id) {
   const std::byte* connector = wireConnector(entity, id);
   const std::byte* copper = connector ? agui::objectAsBase(connector, ".?AVCopperWireConnector@@") : nullptr;
   if (!copper || at<uintptr_t>(copper, layout.connectorWiresEnd) == at<uintptr_t>(copper, layout.connectorWiresBegin))
      return 0;
   const std::byte* network = at<const std::byte*>(copper, layout.copperConnectorNetwork);
   return network ? at<uint32_t>(network, layout.electricNetworkIndex) + 1 : 0;
}

// On or off, and the electric networks on each side (fa-info.lua's ent_info_power_switch).
std::vector<std::string> powerSwitchState(const std::byte* entity) {
   const uint8_t state = at<uint8_t>(entity, layout.powerSwitchState);
   const bool on = state == layout.powerSwitchOn || state == layout.powerSwitchTurningOn;
   std::vector<std::string> items{on ? vocab::kPowerSwitchOn : vocab::kPowerSwitchOff};
   const uint32_t left = copperNetwork(entity, layout.connectorSwitchLeft);
   const uint32_t right = copperNetwork(entity, layout.connectorSwitchRight);
   if (left && right)
      items.push_back(vocab::kPowerSwitchBoth(left, right));
   else if (left)
      items.push_back(vocab::kPowerSwitchLeft(left));
   else if (right)
      items.push_back(vocab::kPowerSwitchRight(right));
   return items;
}

bool isCombinator(std::string_view type) {
   return type == "arithmetic-combinator" || type == "decider-combinator" || type == "selector-combinator";
}

// A combinator's input and output networks by wire (fa-info.lua's ent_info_combinator_connections),
// or any other entity's circuit networks with how many others are on each
// (Circuits.add_circuit_network_info).
std::vector<std::string> circuitNetworks(const std::byte* entity, std::string_view type) {
   std::vector<std::string> items;
   const std::byte* red = circuitNetwork(entity, layout.connectorCircuitRed);
   const std::byte* green = circuitNetwork(entity, layout.connectorCircuitGreen);
   if (isCombinator(type)) {
      auto side = [&](const std::byte* r, const std::byte* g, const vocab::Word& both, const vocab::Word& redOnly,
                      const vocab::Word& greenOnly) {
         if (r && g)
            items.push_back(both(networkId(r), networkId(g)));
         else if (r)
            items.push_back(redOnly(networkId(r)));
         else if (g)
            items.push_back(greenOnly(networkId(g)));
      };
      side(red, green, vocab::kCombinatorInputBoth, vocab::kCombinatorInputRed, vocab::kCombinatorInputGreen);
      side(circuitNetwork(entity, layout.connectorOutputRed), circuitNetwork(entity, layout.connectorOutputGreen),
           vocab::kCombinatorOutputBoth, vocab::kCombinatorOutputRed, vocab::kCombinatorOutputGreen);
      return items;
   }
   auto others = [](const std::byte* network) {
      return static_cast<int64_t>(at<uint32_t>(network, layout.circuitNetworkCircuits)) - 1;
   };
   if (red) items.push_back(vocab::kCircuitRed(networkId(red), others(red)));
   if (green) items.push_back(vocab::kCircuitGreen(networkId(green), others(green)));
   return items;
}

// ---- roboports ----

// A roboport's network and what its cell holds (fa-info.lua's roboport part and
// BotLogistics.roboport_contents_info).
std::vector<std::string> roboportState(const std::byte* roboport) {
   const std::byte* cell = roboport + layout.roboportCell;
   std::vector<std::string> items;
   if (const std::byte* network = at<const std::byte*>(cell, layout.cellNetwork))
      items.push_back(vocab::kRoboportNetwork(networkName(network)));
   const auto* spots = at<const std::byte*>(cell, layout.cellChargingSpots);
   const auto* spotsEnd = at<const std::byte*>(cell, layout.cellChargingSpots + sizeof(void*));
   uint32_t charging = 0;
   for (const std::byte* spot = spots; spot < spotsEnd; spot += layout.chargeSpotSize)
      if (at<const void*>(spot, layout.chargeSpotRobot)) ++charging;
   const std::byte* materials = roboport + layout.roboportMaterials;
   const auto* stacks = at<const std::byte*>(materials, layout.inventoryData);
   uint64_t packs = 0;
   for (uint16_t i = 0; i < at<uint16_t>(materials, layout.inventorySize); ++i)
      packs += at<uint32_t>(stacks + size_t{i} * layout.itemStackSize, layout.itemStackCount);
   items.push_back(vocab::kRoboportCharging(charging, at<uint32_t>(cell, layout.cellWaitingToCharge),
                                            at<uint32_t>(cell, layout.cellStationedLogistic),
                                            at<uint32_t>(cell, layout.cellStationedConstruction), packs));
   return items;
}

// ---- vehicles and trains ----

// An entity's backer name (a locomotive's, a train stop's), or nothing.
std::string backerName(const std::byte* entity) {
   struct MsvcString {
      union {
         char buffer[16];
         const char* pointer;
      };
      size_t size;
      size_t capacity;
   };
   const auto* name = virtualAt<const MsvcString* (*)(const void*)>(entity, layout.entityBackerName)(entity);
   if (!name) return {};
   return text::speakable({name->capacity >= sizeof(name->buffer) ? name->pointer : name->buffer, name->size});
}

// Which of the eight ways a vehicle heads, as FaUtils.get_heading_value rounds its orientation.
uint8_t headingOf(const std::byte* vehicle) {
   // RealOrientation is fixed point: a uint32 of the turn times 2^32.
   uint32_t orientation = 0;
   virtualAt<uint32_t* (*)(const void*, uint32_t*)>(vehicle, layout.entityOrientation)(vehicle, &orientation);
   const auto eighth = static_cast<int>(std::floor(orientation / 4294967296.0 * 8 + 0.5)) % 8;
   return static_cast<uint8_t>(eighth * 2);
}

bool isRollingStock(std::string_view type) {
   return type == "locomotive" || type == "cargo-wagon" || type == "fluid-wagon" || type == "artillery-wagon";
}

const std::byte* trainOf(const std::byte* entity) {
   const std::byte* stock = agui::objectAsBase(entity, ".?AVRollingStock@@");
   return stock ? at<const std::byte*>(stock, layout.rollingStockTrain) : nullptr;
}

// A train's name: its locomotive with the lowest unit number's (TrainHelpers.get_name).
std::string trainName(const std::byte* train) {
   const std::byte* naming = nullptr;
   uint64_t lowest = 0;
   for (const uint32_t movers : {layout.trainFrontMovers, layout.trainBackMovers}) {
      const auto* first = at<const std::byte* const*>(train, movers);
      const auto* last = at<const std::byte* const*>(train, movers + sizeof(void*));
      for (const std::byte* const* loco = first; loco != last; ++loco) {
         const std::byte* owner = agui::objectAsBase(*loco, ".?AVEntityWithOwner@@");
         if (!owner) continue;
         const uint64_t number = at<uint64_t>(owner, layout.entityUnitNumber);
         if (!naming || number < lowest) {
            naming = *loco;
            lowest = number;
         }
      }
   }
   return naming ? backerName(naming) : std::string();
}

// A train driven by hand (TrainHelpers.push_state_message). The stop it heads to is the tooltip's
// next station.
std::string trainState(const std::byte* train) {
   const std::byte* controller = at<const std::byte*>(train, layout.trainController);
   if (!controller) return {};
   if (virtualAt<bool (*)(const void*)>(controller, layout.controllerIsManual)(controller)) return vocab::kTrainManual;
   return {};
}

// ---- drawn icons ----

// The prototypes an entity filters for, as LuaEntity::get_filter reads them (Filters.get_all_filters).
std::vector<const std::byte*> filtersOf(const std::byte* entity) {
   std::vector<const std::byte*> out;
   const std::byte* prototype = prototypeOf(entity);
   auto add = [&](uintptr_t list, uint16_t id) {
      if (const std::byte* filtered = id ? prototypeAt(list, id) : nullptr) out.push_back(filtered);
   };
   auto items = [&](const std::byte* owner, uint32_t filters, uint32_t count) {
      for (uint8_t i = 0; i < at<uint8_t>(prototype, count); ++i)
         add(layout.itemPrototypes,
             at<uint16_t>(owner + filters + size_t{i} * layout.itemFilterSize, layout.itemFilterId));
   };
   if (const std::byte* inserter = agui::objectAsBase(entity, ".?AVInserter@@")) {
      items(inserter, layout.inserterFilters, layout.inserterFilterCount);
   } else if (const std::byte* loader = agui::objectAsBase(entity, ".?AVLoader@@")) {
      items(loader, layout.loaderFilters, layout.loaderFilterCount);
   } else if (const std::byte* drill = agui::objectAsBase(entity, ".?AVMiningDrill@@")) {
      for (uint8_t i = 0; i < at<uint8_t>(prototype, layout.drillFilterCount); ++i)
         add(layout.entityPrototypes, at<uint16_t>(drill, layout.drillFilters + i * uint32_t{sizeof(uint16_t)}));
   } else if (const std::byte* collector = agui::objectAsBase(entity, ".?AVAsteroidCollector@@")) {
      const auto* first = at<const uint16_t*>(collector, layout.collectorFilters);
      const auto* last = at<const uint16_t*>(collector, layout.collectorFilters + sizeof(void*));
      for (const uint16_t* id = first; id != last; ++id) add(layout.asteroidChunkPrototypes, *id);
   } else if (const std::byte* container = agui::objectAsBase(entity, ".?AVLogisticContainer@@")) {
      if (at<uint8_t>(prototype, layout.containerMode) != layout.containerModeStorage) return out;
      const std::byte* point = at<const std::byte*>(container, layout.containerLogisticPoint);
      if (!point) return out;
      const auto* section = at<const std::byte*>(point, layout.pointSections);
      if (section == at<const std::byte*>(point, layout.pointSections + sizeof(void*))) return out;
      const auto* filter = at<const std::byte*>(section, layout.sectionFilters);
      if (filter == at<const std::byte*>(section, layout.sectionFilters + sizeof(void*))) return out;
      using SignalPrototype = const std::byte* (*)(const void* signal);
      if (const std::byte* filtered =
             reinterpret_cast<SignalPrototype>(layout.signalPrototype)(filter + layout.logisticFilterSignal))
         out.push_back(filtered);
   }
   return out;
}

// The internal names of what a recipe makes.
std::vector<std::string_view> productsOf(const std::byte* recipe) {
   std::vector<std::string_view> names;
   auto each = [&](uint32_t products, uint32_t size, uint32_t id, uintptr_t list) {
      const auto* first = at<const std::byte*>(recipe, products);
      const auto* last = at<const std::byte*>(recipe, products + sizeof(void*));
      for (const std::byte* product = first; product < last; product += size)
         if (const std::byte* made = prototypeAt(list, at<uint16_t>(product, id))) names.push_back(internalName(made));
   };
   each(layout.recipeItemProducts, layout.itemProductSize, layout.itemProductId, layout.itemPrototypes);
   each(layout.recipeFluidProducts, layout.fluidProductSize, layout.fluidProductId, layout.fluidPrototypes);
   return names;
}

uint32_t qualityLevel(std::string_view name) {
   const auto& list = *reinterpret_cast<const std::vector<const std::byte*>*>(layout.qualityPrototypes);
   for (const std::byte* quality : list)
      if (quality && internalName(quality) == name) return at<uint32_t>(quality, layout.qualityLevel);
   return 0;
}

// An item or other prototype as ItemInfo.item_or_fluid_info says it: its name, its quality unless
// normal, and a count above one.
std::string itemWords(std::string_view kind, std::string_view name, std::string_view quality, int64_t count) {
   const std::string localised = prototypes::localisedName(kind, name).value_or(std::string(name));
   const bool hasQuality = !quality.empty() && quality != "normal";
   const std::string qualityName =
      quality.empty() ? std::string() : prototypes::localisedName("quality", quality).value_or(std::string(quality));
   return vocab::kItemQuantityQuality(localised, hasQuality ? 1 : 0, qualityName, count > 1 ? 1 : 0, count);
}

// A utility sprite by the mod's words for it ("no_path_icon"), else by its name.
std::string utilityWords(const std::string& name) {
   std::string key = "fa.ent-status-icon-" + name;
   std::ranges::replace(key, '_', '-');
   const std::string said = vocab::translate(key.c_str(), {});
   if (!said.empty() && said.find(key) == std::string::npos) return said;
   std::string plain = name.ends_with("_icon") ? name.substr(0, name.size() - 5) : name;
   std::ranges::replace(plain, '_', ' ');
   return plain;
}

bool isCraftingMachine(std::string_view type) {
   return type == "assembling-machine" || type == "furnace" || type == "rocket-silo";
}

} // namespace

std::vector<std::string> describe(const void* object, int32_t tileX, int32_t tileY) {
   const auto* entity = static_cast<const std::byte*>(object);
   const Belt candidate{entity};
   const Belt belt = candidate.isTransportBelt() || candidate.isUnderground() ? candidate : Belt{};

   // One list item, as the mod's Lua said them: facing, an underground belt's end, a belt's shape,
   // what a belt carries.
   const std::string_view type = typeName(prototypeOf(entity));
   const std::byte* train = isRollingStock(type) ? trainOf(entity) : nullptr;
   std::vector<std::string> first;
   // Rolling stock is named by its train, unless the title says that name already.
   if (train) {
      std::string name = trainName(train);
      if (name != backerName(entity)) first.push_back(std::move(name));
   }
   first.push_back(facing(entity));
   if (type == "locomotive" || type == "car") first.push_back(vocab::kFacing(directionName(headingOf(entity))));
   if (belt && belt.isUnderground())
      first.emplace_back(belt.isUndergroundExit() ? vocab::kUndergroundExit : vocab::kUndergroundEntrance);
   if (belt)
      for (std::string& word : beltShape(belt)) first.push_back(std::move(word));
   if (belt && belt.isTransportBelt()) first.push_back(beltContents(belt));
   first.push_back(combinatorDescription(entity));

   std::vector<std::string> parts{joined(first)};
   if (belt && belt.isUnderground()) parts.push_back(undergroundConnection(belt));
   if (candidate.type() == "splitter") parts.push_back(splitterState(candidate));
   if (isPole(entity)) {
      parts.push_back(poleWires(entity));
      parts.push_back(poleSupply(entity));
   }
   if (type == "logistic-container") parts.push_back(logisticNetwork(entity));
   if (type == "infinity-container")
      for (std::string& item : infinityChestState(entity)) parts.push_back(std::move(item));
   if (type == "infinity-pipe" && !at<uint16_t>(entity, layout.infinityPipeFluid))
      parts.emplace_back(vocab::kInfinityPipeDraining);
   if (isPipe(entity)) parts.push_back(pipeShape(entity));
   for (std::string& item : fluidConnections(entity, Position{tileX, tileY})) parts.push_back(std::move(item));
   if (type == "mining-drill" && isDrillOutput(entity, Position{tileX, tileY})) parts.emplace_back(vocab::kDrillOutput);
   if (const std::byte* signal = agui::objectAsBase(entity, ".?AVRailSignalBase@@"))
      parts.push_back(
         signalState(type == "rail-chain-signal" ? agui::objectAsBase(entity, ".?AVRailChainSignal@@") : signal, type));
   if (const std::byte* wall = agui::objectAsBase(entity, ".?AVWall@@"))
      if (at<const void*>(wall, layout.wallControlBehavior)) parts.emplace_back(vocab::kGateCircuit);
   if (reinterpret_cast<bool (*)(const void*)>(layout.entityToBeUpgraded)(entity))
      parts.emplace_back(vocab::kMarkedForUpgrade);
   if (const std::byte* powerSwitch = agui::objectAsBase(entity, ".?AVPowerSwitch@@"))
      for (std::string& item : powerSwitchState(powerSwitch)) parts.push_back(std::move(item));
   if (const std::byte* roboport = agui::objectAsBase(entity, ".?AVRoboport@@"))
      for (std::string& item : roboportState(roboport)) parts.push_back(std::move(item));
   if (const std::byte* turret = agui::objectAsBase(entity, ".?AVTurret@@")) parts.push_back(turretPriorities(turret));
   if (const std::byte* inserter = agui::objectAsBase(entity, ".?AVInserter@@"))
      parts.push_back(inserterState(inserter));
   if (type == "mining-drill")
      if (const std::byte* target = dropTargetOf(entity)) parts.push_back(vocab::kDrillOutputsTo(targetName(target)));
   for (std::string& item : reactorState(entity)) parts.push_back(std::move(item));
   if (isHeatPipe(entity)) parts.push_back(heatPipeShape(entity));
   parts.push_back(heatConnections(entity, Position{tileX, tileY}));
   for (std::string& item : circuitNetworks(entity, type)) parts.push_back(std::move(item));
   if (train) parts.push_back(trainState(train));
   return parts;
}

std::vector<std::string> drawnIcons(const void* object) {
   const auto* entity = static_cast<const std::byte*>(object);
   const std::optional<entityicons::Drawn> drawn = entityicons::read(entity);
   if (!drawn || drawn->icons.empty()) return {};
   const std::vector<entityicons::Icon>& icons = drawn->icons;
   const std::string_view type = typeName(prototypeOf(entity));
   std::vector<std::string> items;

   struct Group {
      const entityicons::Icon* icon;
      int64_t count;
   };
   auto sameIcon = [](const entityicons::Icon& a, const entityicons::Icon& b) {
      return a.kind == b.kind && a.name == b.name && a.quality == b.quality && a.comparison == b.comparison &&
             a.anyQuality == b.anyQuality && a.denied == b.denied;
   };
   auto grouped = [&](const std::vector<const entityicons::Icon*>& from) {
      std::vector<Group> groups;
      for (const entityicons::Icon* icon : from) {
         auto found = std::ranges::find_if(groups, [&](const Group& group) { return sameIcon(*group.icon, *icon); });
         if (found != groups.end())
            ++found->count;
         else
            groups.push_back({icon, 1});
      }
      return groups;
   };

   // A beacon's modules, with counts, most first (InventoryUtils.present_list).
   if (type == "beacon") {
      std::vector<const entityicons::Icon*> modules;
      for (const entityicons::Icon& icon : icons)
         if (icon.kind == "item") modules.push_back(&icon);
      std::vector<Group> groups = grouped(modules);
      std::ranges::sort(groups, [](const Group& a, const Group& b) {
         if (a.count != b.count) return a.count > b.count;
         if (a.icon->name != b.icon->name) return a.icon->name > b.icon->name;
         return qualityLevel(a.icon->quality) > qualityLevel(b.icon->quality);
      });
      std::vector<std::string> words;
      for (const Group& group : groups)
         words.push_back(itemWords("item", group.icon->name, group.icon->quality, group.count));
      if (!words.empty()) items.push_back(vocab::kInventoryPresentation(joined(words, ", ")));
      return items;
   }

   std::vector<const std::byte*> filters;
   if (type != "infinity-container") filters = filtersOf(entity);
   auto isFilter = [&](const entityicons::Icon& icon) {
      return !icon.kind.empty() &&
             std::ranges::any_of(filters, [&](const std::byte* filter) { return internalName(filter) == icon.name; });
   };

   // A crafting machine draws its recipe as the recipe's own icon or, without one, its main product's.
   const std::byte* recipe = nullptr;
   if (isCraftingMachine(type))
      if (const std::byte* machine = agui::objectAsBase(entity, ".?AVCraftingMachine@@"))
         if (const uint16_t id = at<uint16_t>(machine + layout.craftingRecipe, layout.idWithQualityBase))
            recipe = prototypeAt(layout.recipePrototypes, id);
   const std::vector<std::string_view> products = recipe ? productsOf(recipe) : std::vector<std::string_view>{};
   auto showsRecipe = [&](const entityicons::Icon& icon) {
      return recipe && (icon.kind == "recipe" || std::ranges::find(products, icon.name) != products.end());
   };

   // The fluids drawn, once each; the first icon that shows the recipe is the recipe's.
   std::vector<std::string> fluids;
   bool recipeIcon = false;
   for (const entityicons::Icon& icon : icons) {
      if (!recipeIcon && showsRecipe(icon)) {
         recipeIcon = true;
      } else if (icon.kind == "fluid") {
         std::string name = prototypes::localisedName("fluid", icon.name).value_or(icon.name);
         if (std::ranges::find(fluids, name) == fluids.end()) fluids.push_back(std::move(name));
      }
   }

   bool saidRecipe = false;
   bool saidFluids = false;
   bool saidFilters = false;
   std::vector<const entityicons::Icon*> others;
   for (const entityicons::Icon& icon : icons) {
      if (!saidRecipe && showsRecipe(icon)) {
         saidRecipe = true;
         items.push_back(joined({vocab::kProducing, localisedName(recipe)}));
      } else if (icon.kind == "fluid") {
         if (!saidFluids && !fluids.empty()) items.push_back(vocab::kInventoryPresentation(joined(fluids, ", ")));
         saidFluids = true;
      } else if (isFilter(icon)) {
         if (!saidFilters) {
            // A filter that denies is drawn crossed out.
            const bool denies = std::ranges::any_of(
               icons, [&](const entityicons::Icon& drawnIcon) { return drawnIcon.denied && isFilter(drawnIcon); });
            for (size_t i = 0; i < filters.size(); ++i) {
               const std::string name = localisedName(filters[i]);
               items.push_back(i == 0 ? joined({denies ? vocab::kFilterDenies : vocab::kFilterAllows, name}) : name);
            }
         }
         saidFilters = true;
      } else {
         others.push_back(&icon);
      }
   }

   for (const Group& group : grouped(others)) {
      const entityicons::Icon& icon = *group.icon;
      std::vector<std::string> words{icon.comparison};
      if (!icon.kind.empty())
         words.push_back(itemWords(icon.kind, icon.name, icon.quality, icon.kind == "item" ? group.count : 0));
      else
         words.push_back(utilityWords(icon.name));
      if (icon.anyQuality) words.emplace_back(vocab::kAnyQuality);
      if (icon.denied) words.emplace_back(vocab::kIconDenied);
      items.push_back(joined(words));
   }
   return items;
}

} // namespace fa::facts
