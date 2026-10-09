#include "facts.h"

#include "agui.h"
#include "game.h"
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
   struct MsvcString {
      union {
         char buffer[16];
         const char* pointer;
      };
      size_t size;
      size_t capacity;
   };
   const auto& name = *reinterpret_cast<const MsvcString*>(prototypeOf(entity) + layout.prototypeName);
   if (std::string_view(name.capacity >= 16 ? name.pointer : name.buffer, name.size) != "nuclear-reactor") return out;
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

} // namespace

std::vector<std::string> describe(const void* object, int32_t tileX, int32_t tileY) {
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
   if (isPole(entity)) {
      parts.push_back(poleWires(entity));
      parts.push_back(poleSupply(entity));
   }
   if (isPipe(entity)) parts.push_back(pipeShape(entity));
   for (std::string& item : fluidConnections(entity, Position{tileX, tileY})) parts.push_back(std::move(item));
   for (std::string& item : reactorState(entity)) parts.push_back(std::move(item));
   if (isHeatPipe(entity)) parts.push_back(heatPipeShape(entity));
   parts.push_back(heatConnections(entity, Position{tileX, tileY}));
   return parts;
}

} // namespace fa::facts
