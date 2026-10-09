#include "scanner.h"

#include "bindings.h"
#include "chart.h"
#include "game.h"
#include "log.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <exception>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace fa::scanner {

namespace {

using game::layout;
using Clock = std::chrono::steady_clock;

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

template <class T>
struct MsvcVector {
   T* first;
   T* last;
   T* end;
};

struct MsvcString {
   union {
      char buffer[16];
      char* pointer;
   };
   size_t size;
   size_t capacity;
};

std::string_view view(const MsvcString& string) {
   return {string.capacity >= sizeof(string.buffer) ? string.pointer : string.buffer, string.size};
}

// MapPosition, in 1/256 of a tile.
struct Position {
   int32_t x;
   int32_t y;
   bool operator==(const Position&) const = default;
};

int32_t fixedPoint(double tiles) { return static_cast<int32_t>(std::lround(tiles * game::kMapPositionScale)); }
double tiles(int32_t fixed) { return static_cast<double>(fixed) / game::kMapPositionScale; }

// The tile a position is on.
int32_t tileOf(int32_t fixed) { return fixed >> 8; }

// The centre of a tile, as a position.
Position tileCentre(int32_t x, int32_t y) { return {x * 256 + 128, y * 256 + 128}; }

// The categories in the order the category keys move through them, by the keys of scanner-consts.lua
// CATEGORIES, which name them in the mod's locale (fa.scanner-category-<key>).
enum Cat : uint8_t {
   All,
   BuildSpots,
   Pins,
   Tags,
   Resources,
   Enemies,
   Remnants,
   Production,
   Logistics,
   Containers,
   Military,
   Vehicles,
   Spidertrons,
   Trains,
   Ghosts,
   Players,
   Corpses,
   Other,
   Terrain,
   kCategoryCount,
};
constexpr std::array<std::string_view, kCategoryCount> kCategoryKeys{
   "all",         "build_spots", "pins",
   "tags",        "resources",   "enemies",
   "remnants",    "production",  "logistics_and_power",
   "containers",  "military",    "vehicles",
   "spidertrons", "trains",      "ghosts",
   "players",     "corpses",     "other",
   "terrain",
};
static_assert(!kCategoryKeys.back().empty(), "a category has no key");

// The category of each entity type the scanner lists one by one. Types left out (beams,
// explosions, particles ...) are not listed; trees and resources are listed as forests and patches.
struct TypeRule {
   std::string_view type;
   Cat category;
};
constexpr TypeRule kTypes[] = {
   {"accumulator", Logistics},
   {"ammo-turret", Military},
   {"arithmetic-combinator", Logistics},
   {"artillery-flare", Military},
   {"artillery-turret", Military},
   {"artillery-wagon", Trains},
   {"assembling-machine", Production},
   {"beacon", Production},
   {"boiler", Logistics},
   {"burner-generator", Logistics},
   {"car", Vehicles},
   {"cargo-wagon", Trains},
   {"character-corpse", Other},
   {"character", Players},
   {"cliff", Terrain},
   {"combat-robot", Military},
   {"constant-combinator", Logistics},
   {"construction-robot", Logistics},
   {"container", Containers},
   {"corpse", Corpses},
   {"curved-rail-a", Trains},
   {"curved-rail-b", Trains},
   {"decider-combinator", Logistics},
   {"electric-energy-interface", Logistics},
   {"electric-pole", Logistics},
   {"electric-turret", Military},
   {"elevated-curved-rail-a", Trains},
   {"elevated-curved-rail-b", Trains},
   {"elevated-half-diagonal-rail", Trains},
   {"elevated-straight-rail", Trains},
   {"entity-ghost", Ghosts},
   {"fire", Other},
   {"fish", Other},
   {"flame-thrower-explosion", Other},
   {"fluid-turret", Military},
   {"fluid-wagon", Trains},
   {"furnace", Production},
   {"gate", Military},
   {"generator", Logistics},
   {"half-diagonal-rail", Trains},
   {"heat-interface", Logistics},
   {"heat-pipe", Logistics},
   {"infinity-container", Containers},
   {"infinity-pipe", Logistics},
   {"inserter", Logistics},
   {"item-entity", Other},
   {"lab", Production},
   {"lamp", Logistics},
   {"legacy-curved-rail", Trains},
   {"legacy-straight-rail", Trains},
   {"land-mine", Military},
   {"linked-belt", Logistics},
   {"linked-container", Logistics},
   {"loader-1x1", Logistics},
   {"loader", Logistics},
   {"locomotive", Trains},
   {"logistic-container", Containers},
   {"logistic-robot", Logistics},
   {"market", Logistics},
   {"mining-drill", Production},
   {"offshore-pump", Production},
   {"pipe-to-ground", Logistics},
   {"pipe", Logistics},
   {"player-port", Other},
   {"power-switch", Logistics},
   {"programmable-speaker", Logistics},
   {"projectile", Other},
   {"pump", Logistics},
   {"radar", Military},
   {"rail-chain-signal", Trains},
   {"rail-ramp", Trains},
   {"rail-remnants", Remnants},
   {"rail-signal", Trains},
   {"rail-support", Trains},
   {"reactor", Logistics},
   {"roboport", Logistics},
   {"rocket-silo-rocket-shadow", Other},
   {"rocket-silo-rocket", Other},
   {"rocket-silo", Production},
   {"simple-entity-with-force", Other},
   {"simple-entity-with-owner", Other},
   {"simple-entity", Other},
   {"solar-panel", Logistics},
   {"spider-vehicle", Spidertrons},
   {"splitter", Logistics},
   {"storage-tank", Logistics},
   {"straight-rail", Trains},
   {"tile-ghost", Ghosts},
   {"train-stop", Trains},
   {"transport-belt", Logistics},
   {"turret", Enemies},
   {"underground-belt", Logistics},
   {"unit-spawner", Enemies},
   {"unit", Enemies},
   {"wall", Military},
};

// Rocks are resources: they are mined for stone and coal.
constexpr std::string_view kRocks[] = {
   "big-rock",         "big-sand-rock", "huge-rock",       "medium-rock",
   "medium-sand-rock", "small-rock",    "small-sand-rock", "tiny-rock",
};

// What sets an entity apart from others of its prototype in its subcategory: what a machine makes,
// what a drill mines, what a chest or pipe holds, which train a wagon is in, how polluted a spawner
// is, which network a roboport names.
enum class Detail : uint8_t {
   None,
   Recipe,
   Furnace,
   Drill,
   Train,
   Ghost,
   TileGhost,
   Spawner,
   Contents,
   Fluid,
   Pipe,
   Roboport
};

struct DetailRule {
   std::string_view type;
   Detail detail;
   uint32_t game::Layout::* member; // where the entity class keeps what is read
};
constexpr DetailRule kDetails[] = {
   {"artillery-wagon", Detail::Train, &game::Layout::scanArtilleryWagonTrain},
   {"assembling-machine", Detail::Recipe, &game::Layout::scanAssemblerRecipe},
   {"cargo-wagon", Detail::Train, &game::Layout::scanCargoWagonTrain},
   {"container", Detail::Contents, &game::Layout::scanContainerInventory},
   {"entity-ghost", Detail::Ghost, &game::Layout::scanGhostInner},
   {"fluid-wagon", Detail::Train, &game::Layout::scanFluidWagonTrain},
   {"furnace", Detail::Furnace, &game::Layout::scanFurnaceRecipe},
   {"infinity-container", Detail::Contents, &game::Layout::scanInfinityInventory},
   {"infinity-pipe", Detail::Fluid, &game::Layout::scanInfinityPipeFluidBox},
   {"locomotive", Detail::Train, &game::Layout::scanLocomotiveTrain},
   {"logistic-container", Detail::Contents, &game::Layout::scanLogisticInventory},
   {"mining-drill", Detail::Drill, &game::Layout::scanDrillResources},
   {"pipe", Detail::Pipe, &game::Layout::scanPipeFluidBox},
   {"pipe-to-ground", Detail::Fluid, &game::Layout::scanUndergroundFluidBox},
   {"roboport", Detail::Roboport, &game::Layout::scanRoboportName},
   {"storage-tank", Detail::Fluid, &game::Layout::scanTankFluidBox},
   {"tile-ghost", Detail::TileGhost, nullptr},
   {"unit-spawner", Detail::Spawner, &game::Layout::scanSpawnerPollution},
};

// How an entity is listed.
enum class Listing : uint8_t { No, Alone, Tree, Resource };

struct Rule {
   Listing listing = Listing::No;
   Cat category = Other;
   Detail detail = Detail::None;
   uint32_t member = 0;   // the detail's offset in the entity
   bool infinite = false; // a resource that never runs out, such as crude oil
   int8_t cellShift = -1; // a resource's patch cells, log2 of their side in tiles; -1 when each is a patch
};

// The side of a resource's patch cells as the map has it (see game.h): log2 of it, or -1 when every
// resource is a patch of its own.
int8_t patchCellShift(const std::byte* prototype) {
   uint32_t side = std::min<uint32_t>(at<uint32_t>(prototype, layout.resourceSearchRadius) * 2, 32);
   if (side == 0) return -1;
   while (32 % side != 0) --side;
   return static_cast<int8_t>(std::countr_zero(side));
}

Rule ruleFor(const std::byte* prototype, std::string_view type, std::string_view name) {
   if (type == "tree") return {Listing::Tree, Resources};
   if (type == "resource") {
      Rule rule{Listing::Resource, Resources};
      rule.infinite = at<bool>(prototype, layout.resourceInfinite);
      rule.cellShift = patchCellShift(prototype);
      return rule;
   }
   if (std::find(std::begin(kRocks), std::end(kRocks), name) != std::end(kRocks)) return {Listing::Alone, Resources};
   if (name.ends_with("-remnants")) return {Listing::Alone, Remnants};
   for (const TypeRule& typeRule : kTypes) {
      if (typeRule.type != type) continue;
      Rule rule{Listing::Alone, typeRule.category};
      for (const DetailRule& detail : kDetails)
         if (detail.type == type) {
            rule.detail = detail.detail;
            rule.member = detail.member ? layout.*detail.member : 0;
         }
      return rule;
   }
   return {};
}

// scanner-consts.lua: how close trees and the wells of an infinite resource are listed one by one
// rather than in their forest or field, in tiles.
constexpr double kForestZoomDistance = 25;
constexpr double kInfiniteResourceZoomDistance = 50;
// Trees in the same or touching cells of this many tiles a side are one forest.
constexpr int kForestCellShift = 3;

const std::byte* currentGame() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   return context ? at<const std::byte*>(context, layout.globalGame) : nullptr;
}

// This client's player when it is LuaPlayer `playerIndex`, else null.
const std::byte* localPlayer(const std::byte* game, int playerIndex) {
   if (!game) return nullptr;
   const std::byte* player = at<const std::byte*>(game, layout.gameLocalPlayer);
   if (!player || at<uint16_t>(player, layout.playerIndex) + 1 != playerIndex) return nullptr;
   return player;
}

// The surface with LuaSurface::index `index` (one more than SurfaceIndex) on `map`, or null once it
// is gone.
const std::byte* surfaceAt(const std::byte* map, uint32_t index) {
   const auto& surfaces = at<MsvcVector<const std::byte*>>(map, layout.mapSurfaces);
   for (const std::byte* const* surface = surfaces.first; surface < surfaces.last; ++surface)
      if (*surface && at<uint32_t>(*surface, layout.surfaceIndex) + 1 == index) return *surface;
   return nullptr;
}

// The player's force, for what it charted and how it sees resource patches.
const std::byte* forceOf(const std::byte* player) {
   const std::byte* map = at<const std::byte*>(player, layout.playerMap);
   return at<const std::byte* const*>(map, layout.mapForces)[at<uint8_t>(player, layout.playerForce)];
}

std::string_view prototypeName(const std::byte* prototype) {
   return view(at<MsvcString>(prototype, layout.prototypeName));
}

std::string_view prototypeType(const std::byte* prototype) {
   return virtualAt<const char* (*)(const void*)>(prototype, layout.prototypeGetType)(prototype);
}

// The game's TargeterBase: a weak reference to an entity, which the game nulls when the entity goes.
// Moving it would break the list it is linked into, so it never moves while linked.
struct Targeter {
   const void* vfptr;
   std::byte* target;
   Targeter* next;
   Targeter* previous;
};
static_assert(sizeof(Targeter) == 32);

Targeter*& firstTargeter(std::byte* targetable) {
   return *reinterpret_cast<Targeter**>(targetable + layout.targetableTargeters);
}

// As TargeterBase's constructor links one.
void link(Targeter& targeter, std::byte* entity) {
   targeter.vfptr = reinterpret_cast<const void*>(layout.entityTargeterVtable);
   targeter.target = entity;
   targeter.previous = nullptr;
   Targeter*& first = firstTargeter(entity);
   targeter.next = first;
   if (first) first->previous = &targeter;
   first = &targeter;
}

// As TargeterBase's destructor unlinks one.
void unlink(Targeter& targeter) {
   if (!targeter.target) return;
   if (targeter.previous)
      targeter.previous->next = targeter.next;
   else
      firstTargeter(targeter.target) = targeter.next;
   if (targeter.next) targeter.next->previous = targeter.previous;
   targeter.target = nullptr;
}

// The weak references of a list's entities, made and dropped while the world stands still (in a
// refresh, or at a key). They are dropped from the game they were made in; once that game is gone
// they are left as they are, since their entities may be gone without having told them. They never
// move while linked: a deque keeps them in place as more are added.
class Links {
public:
   Links() = default;
   explicit Links(const std::byte* game) : game_(game) {}
   Links(Links&& other) noexcept : targeters_(std::move(other.targeters_)), game_(other.game_) {}
   Links& operator=(Links&& other) noexcept {
      if (this != &other) {
         release();
         targeters_ = std::move(other.targeters_);
         game_ = other.game_;
      }
      return *this;
   }
   Links(const Links&) = delete;
   Links& operator=(const Links&) = delete;
   ~Links() { release(); }

   // Links `entity`, which must be there now; its index.
   uint32_t add(std::byte* entity) {
      link(targeters_.emplace_back(), entity);
      return static_cast<uint32_t>(targeters_.size() - 1);
   }

   // Entity `index`, or null once the game dropped it.
   const std::byte* entity(size_t index) const { return targeters_[index].target; }

   size_t size() const { return targeters_.size(); }

   // Drops links until `deadline`, so that a big list's go over several ticks; whether all are gone.
   bool releaseUntil(Clock::time_point deadline) {
      if (game_ != currentGame()) {
         release();
         return true;
      }
      for (size_t dropped = 0; !targeters_.empty(); ++dropped) {
         if (dropped % 4096 == 0 && Clock::now() >= deadline) return false;
         unlink(targeters_.back());
         targeters_.pop_back();
      }
      return true;
   }

private:
   void release() {
      if (targeters_.empty()) return;
      if (game_ == currentGame()) {
         for (Targeter& targeter : targeters_) unlink(targeter);
      } else {
         // Still linked into a game that may not have let go of them: keep them in place for good.
         new std::deque<Targeter>(std::move(targeters_));
      }
      targeters_.clear();
   }

   std::deque<Targeter> targeters_;
   const std::byte* game_ = nullptr;
};

// Spawners are grouped by how much pollution they absorbed, at these thresholds: those of
// scripts/scanner/readout.lua's SPAWNER_POLLUTION_BUCKETS, which says the group.
constexpr double kSpawnerPollution[] = {0, 1, 99};

// The fluid in a fluid box, by ID, or 0 for none.
uint16_t fluidIn(const std::byte* box) {
   if (const std::byte* segment = at<const std::byte*>(box, layout.fluidBoxSegment))
      return at<int64_t>(segment, layout.segmentAmount) > 0 ? at<uint16_t>(segment, layout.segmentFluid) : 0;
   return at<int64_t>(box, layout.fluidBoxAmount) > 0 ? at<uint16_t>(box, layout.fluidBoxFluid) : 0;
}

// An entity's subcategory: its prototype's name and what sets it apart from others of its prototype.
// Only grouped by, never said.
std::string subcategoryKey(const std::byte* entity, const std::byte* prototype, const Rule& rule) {
   std::string key(prototypeName(prototype));
   switch (rule.detail) {
   case Detail::None: break;
   case Detail::Recipe: key += std::format("/{}", at<uint16_t>(entity + rule.member, layout.idWithQualityBase)); break;
   case Detail::Furnace: {
      // One without a recipe yet may still hold what it made.
      if (const uint16_t recipe = at<uint16_t>(entity + rule.member, layout.idWithQualityBase)) {
         key += std::format("/{}", recipe);
         break;
      }
      const std::byte* result = entity + layout.scanFurnaceResult;
      const std::byte* stacks = at<const std::byte*>(result, layout.inventoryData);
      if (at<uint16_t>(result, layout.inventorySize) > 0 && at<uint32_t>(stacks, layout.itemStackCount) > 0)
         key += std::format("/item{}", at<uint16_t>(stacks, layout.itemStackItem));
      break;
   }
   case Detail::Drill: {
      const auto& targeters = at<MsvcVector<const std::byte>>(entity, rule.member);
      std::vector<std::string_view> resources;
      for (const std::byte* targeter = targeters.first; targeter < targeters.last;
           targeter += layout.resourceTargeterSize)
         if (const auto* resource = at<const std::byte*>(targeter, offsetof(Targeter, target)))
            resources.push_back(prototypeName(at<const std::byte*>(resource, layout.entityPrototypeOf)));
      std::sort(resources.begin(), resources.end());
      resources.erase(std::unique(resources.begin(), resources.end()), resources.end());
      for (std::string_view resource : resources) key += std::format("/{}", resource);
      break;
   }
   case Detail::Train:
      // Wagons are grouped by train, so that the scanner isn't cluttered with every wagon.
      if (const std::byte* train = at<const std::byte*>(entity, rule.member))
         return std::format("train/{}", at<uint32_t>(train, layout.trainId));
      break;
   case Detail::Ghost:
      if (const std::byte* inner = at<const std::byte*>(entity, rule.member))
         return std::format("ghost/{}", prototypeType(at<const std::byte*>(inner, layout.entityPrototypeOf)));
      break;
   case Detail::TileGhost: return "ghost/tile";
   case Detail::Spawner: {
      const double pollution = at<double>(entity, rule.member);
      size_t level = 0;
      while (level + 1 < std::size(kSpawnerPollution) && kSpawnerPollution[level + 1] <= pollution) ++level;
      key += std::format("/{}", level);
      break;
   }
   case Detail::Contents: {
      // Nothing, one item (of any qualities), or a mix.
      std::optional<uint16_t> item;
      bool mixed = false;
      if (const std::byte* inventory = at<const std::byte*>(entity, rule.member)) {
         const std::byte* stacks = at<const std::byte*>(inventory, layout.inventoryData);
         for (uint16_t i = 0; i < at<uint16_t>(inventory, layout.inventorySize) && !mixed; ++i) {
            const std::byte* stack = stacks + size_t{i} * layout.itemStackSize;
            if (at<uint32_t>(stack, layout.itemStackCount) == 0) continue;
            const uint16_t id = at<uint16_t>(stack, layout.itemStackItem);
            if (item && *item != id) mixed = true;
            item = id;
         }
      }
      key += mixed ? std::string("/mixed") : item ? std::format("/item{}", *item) : std::string("/empty");
      break;
   }
   case Detail::Fluid: key += std::format("/{}", fluidIn(entity + rule.member)); break;
   case Detail::Pipe: {
      // A pipe that ends: connected on one side only.
      const std::byte* box = entity + rule.member;
      const auto* first = at<const std::byte*>(box, layout.fluidBoxConnectionsBegin);
      const auto* last = at<const std::byte*>(box, layout.fluidBoxConnectionsEnd);
      int connected = 0;
      for (const std::byte* connection = first; connection < last; connection += layout.fluidConnectionSize)
         if (at<const std::byte*>(connection, layout.fluidConnectionTarget)) ++connected;
      key += std::format("/{}{}", fluidIn(box), connected == 1 ? "/end" : "");
      break;
   }
   case Detail::Roboport: key += std::format("/{}", view(at<MsvcString>(entity, rule.member))); break;
   }
   return key;
}

using IteratorFunction = void (*)(std::byte* iterator);

// Calls `visit` with the entities on `surface` in the advanced tiles (two tiles a side) from `first`
// to `last`. Some stand up to about a tile past the area's edge, and the area beside it gives them
// again.
template <class Visit>
void forEachEntity(const std::byte* surface, Position first, Position last, Visit&& visit) {
   alignas(8) std::byte iterator[game::kEntityIteratorCapacity]{};
   std::memcpy(iterator + layout.iteratorSurface, &surface, sizeof(surface));
   std::memcpy(iterator + layout.iteratorLeftTop, &first, sizeof(first));
   std::memcpy(iterator + layout.iteratorRightBottom, &last, sizeof(last));
   std::memcpy(iterator + layout.iteratorCurrentTile, &first, sizeof(first));
   reinterpret_cast<IteratorFunction>(layout.iteratorStartTile)(iterator);
   for (reinterpret_cast<IteratorFunction>(layout.iteratorMove)(iterator);;
        reinterpret_cast<IteratorFunction>(layout.iteratorMove)(iterator)) {
      const std::byte* entity = at<const std::byte*>(iterator, layout.iteratorCurrentEntity);
      if (!entity) return;
      visit(entity);
   }
}

// Entity::usageBitMask bits of entities the Lua API never hands out (see game.h).
constexpr uint16_t kNotListedBits = 0x4 | 0x10;

using ChartedFunction = bool (*)(const void* force, uint32_t surfaceIndex, const Position* position);

// TilePosition: two ints.
struct TilePosition {
   int32_t x;
   int32_t y;
};
using TileAtFunction = const std::byte* (*)(const void* surface, const TilePosition* position);

// The tile ID at `x`, `y` (tiles) on `surface`, if a chunk is there.
std::optional<uint16_t> tileAt(const std::byte* surface, int32_t x, int32_t y) {
   const TilePosition position{x, y};
   const std::byte* tile = reinterpret_cast<TileAtFunction>(layout.surfaceTileAt)(surface, &position);
   if (!tile) return std::nullopt;
   return at<uint16_t>(tile, 0);
}

using PatchConstructFunction = void* (*)(void* info, bool useClockLimiter);
using PatchDestroyFunction = void (*)(void* info);
using PatchUpdateFunction = bool (*)(void* info, const void* resource, const void* force, bool keepIfUnchanged);

// A ResourcePatchInfo of our own, for as long as one announcement needs it.
class PatchFinder {
public:
   explicit PatchFinder(const std::byte* force) : force_(force) {
      reinterpret_cast<PatchConstructFunction>(layout.patchInfoConstruct)(info_, false);
   }
   ~PatchFinder() { reinterpret_cast<PatchDestroyFunction>(layout.patchInfoDestroy)(info_); }
   PatchFinder(const PatchFinder&) = delete;
   PatchFinder& operator=(const PatchFinder&) = delete;

   // Finds the patch `resource` is in, as the map does.
   void find(const std::byte* resource) {
      reinterpret_cast<PatchUpdateFunction>(layout.patchInfoUpdate)(info_, resource, force_, false);
   }

   // The map's label of the patch found last.
   std::string label(const std::byte* resource) const { return chart::patchLabel(info_, resource); }

private:
   alignas(16) std::byte info_[game::kPatchInfoCapacity];
   const std::byte* force_;
};

// FaUtils.get_direction_biased: the 8-way defines.direction (16-way numbering) from `origin` to
// `target`, by whole tiles, keeping to a cardinal unless the diagonal is clear.
int directionBiased(Position target, Position origin) {
   int dx = tileOf(target.x) - tileOf(origin.x);
   int dy = tileOf(target.y) - tileOf(origin.y);
   constexpr int north = 0, northeast = 2, east = 4, southeast = 6, south = 8, southwest = 10, west = 12,
                 northwest = 14;
   if (std::abs(dx) > 4 * std::abs(dy)) return dx > 0 ? east : west;
   if (std::abs(dy) > 4 * std::abs(dx)) return dy > 0 ? south : north;
   if (dx > 0 && dy > 0) return southeast;
   if (dx > 0 && dy < 0) return northeast;
   if (dx < 0 && dy > 0) return southwest;
   if (dx < 0 && dy < 0) return northwest;
   return north;
}

double distanceSquared(Position a, Position b) {
   double dx = tiles(a.x) - tiles(b.x);
   double dy = tiles(a.y) - tiles(b.y);
   return dx * dx + dy * dy;
}

// Whether `position`, `distance` from the origin, comes before the nearest so far. Ties go to the
// topmost, then the leftmost, so that the list does not depend on how the walk was shared out.
bool nearer(double distance, Position position, double nearest, Position nearestPosition) {
   if (distance != nearest) return distance < nearest;
   return std::tie(position.y, position.x) < std::tie(nearestPosition.y, nearestPosition.x);
}

// Disjoint sets, for joining touching cells and tiles into forests and bodies of water.
class Sets {
public:
   uint32_t add() {
      parent_.push_back(static_cast<uint32_t>(parent_.size()));
      return parent_.back();
   }
   uint32_t find(uint32_t a) {
      while (parent_[a] != a) {
         parent_[a] = parent_[parent_[a]];
         a = parent_[a];
      }
      return a;
   }
   void join(uint32_t a, uint32_t b) {
      a = find(a);
      b = find(b);
      if (a != b) parent_[b] = a;
   }
   uint32_t size() const { return static_cast<uint32_t>(parent_.size()); }

private:
   std::vector<uint32_t> parent_;
};

uint64_t packCell(int32_t x, int32_t y) {
   return static_cast<uint32_t>(x) | static_cast<uint64_t>(static_cast<uint32_t>(y)) << 32;
}

// A rectangle of whole tiles, both corners in it.
struct TileBox {
   int32_t left = INT32_MAX;
   int32_t top = INT32_MAX;
   int32_t right = INT32_MIN;
   int32_t bottom = INT32_MIN;
   void add(int32_t x, int32_t y) {
      left = std::min(left, x);
      top = std::min(top, y);
      right = std::max(right, x);
      bottom = std::max(bottom, y);
   }
   void add(const TileBox& other) {
      left = std::min(left, other.left);
      top = std::min(top, other.top);
      right = std::max(right, other.right);
      bottom = std::max(bottom, other.bottom);
   }
   int32_t width() const { return right - left + 1; }
   int32_t height() const { return bottom - top + 1; }
};

// What an entry stands for.
enum class Kind : uint8_t {
   Entity, // one entity
   Forest, // trees, nearest first
   Patch,  // a resource patch's resources, nearest first
   Water,  // a body of water, by its tile nearest the origin
   Ice,    // a body of ice, likewise
   Extra,  // one the mod listed itself, by its place in Refresh::extras (Item::first)
};

constexpr std::string_view kindName(Kind kind) {
   switch (kind) {
   case Kind::Entity: return "entity";
   case Kind::Forest: return "forest";
   case Kind::Patch: return "patch";
   case Kind::Water: return "water";
   case Kind::Ice: return "ice";
   case Kind::Extra: return "extra";
   }
   return {};
}

std::optional<Cat> categoryByKey(std::string_view key) {
   for (size_t i = 0; i < kCategoryKeys.size(); ++i)
      if (kCategoryKeys[i] == key) return static_cast<Cat>(i);
   return std::nullopt;
}

struct Item {
   Kind kind;
   Cat category;
   const std::byte* prototype = nullptr; // an entity's or patch's prototype
   Position position{};             // where to go: kept up to date as the entry is checked
   std::string key;                 // its subcategory
   uint32_t first = 0;              // an entity's link in Links
   uint32_t count = 0;              // links of an entity; trees of a forest, resources of a patch
   TileBox box;                     // water, ice, forests and patches
};

struct Subcategory {
   std::string key;
   std::vector<uint32_t> items; // into List::items
};

// What a tile is to the scanner.
enum TileClass : uint8_t { NoClass, WaterTile, IceTile };

struct List {
   const std::byte* game = nullptr;
   uint32_t surfaceIndex = 0;
   const std::byte* force = nullptr;
   Position origin{};
   std::optional<int> direction; // only entries this way from the origin
   uint32_t generation = 0;      // the mod's extras (Refresh::generation)
   std::vector<Item> items;
   Links links;
   std::array<std::vector<Subcategory>, kCategoryCount> categories;
   std::vector<uint8_t> tileClasses; // TileClass by tile ID
};

constexpr unsigned kMaxThreads = 16;

// How many threads `count` pieces of work take, a thread for each `perThread` of them.
unsigned threadsFor(size_t count, size_t perThread) {
   return static_cast<unsigned>(
      std::clamp<size_t>(count / perThread, 1, std::clamp(std::thread::hardware_concurrency(), 1u, kMaxThreads)));
}

// The threads the scanner's work runs on, made as first needed and kept: a refresh spread over ticks
// hands them work every tick.
class Pool {
public:
   // Runs `task(thread)` on `threads` threads, this one as thread 0, and returns once all are done.
   // `task` must not throw. One run at a time.
   void run(unsigned threads, const std::function<void(unsigned)>& task) {
      std::scoped_lock serial(runMutex_);
      if (threads <= 1) {
         task(0);
         return;
      }
      {
         std::scoped_lock lock(mutex_);
         while (workers_.size() + 1 < threads) {
            const auto index = static_cast<unsigned>(workers_.size() + 1);
            workers_.emplace_back([this, index] { serve(index); });
         }
         task_ = &task;
         helpers_ = threads - 1;
         remaining_ = threads - 1;
         ++round_;
      }
      wake_.notify_all();
      task(0);
      std::unique_lock lock(mutex_);
      done_.wait(lock, [&] { return remaining_ == 0; });
   }

private:
   void serve(unsigned index) {
      uint64_t seen = 0;
      std::unique_lock lock(mutex_);
      for (;;) {
         wake_.wait(lock, [&] { return round_ != seen; });
         seen = round_;
         if (index > helpers_) continue;
         const auto* task = task_;
         lock.unlock();
         (*task)(index);
         lock.lock();
         if (--remaining_ == 0) done_.notify_one();
      }
   }

   std::mutex runMutex_;
   std::mutex mutex_;
   std::condition_variable wake_;
   std::condition_variable done_;
   std::vector<std::thread> workers_;
   const std::function<void(unsigned)>* task_ = nullptr;
   unsigned helpers_ = 0;
   unsigned remaining_ = 0;
   uint64_t round_ = 0;
};

// Never destroyed: its threads wait for work until the process ends, as joining them while the DLL
// unloads could hang.
Pool& pool() {
   static Pool& instance = *new Pool;
   return instance;
}

// Calls `work(thread, i)` for `i` from `first` up to `count` on `threads` threads, this one among
// them, handing out `batch` at a time; past `deadline`, if any, no thread takes another batch.
// Returns where it stopped: every `i` below that is done. Work that reads the game must only read
// it, and only while it stands still.
template <class Work>
size_t parallelFor(unsigned threads, size_t first, size_t count, size_t batch, const Work& work,
                   std::optional<Clock::time_point> deadline = std::nullopt) {
   std::atomic<size_t> next{first};
   std::vector<std::exception_ptr> failures(threads);
   pool().run(threads, [&](unsigned thread) {
      try {
         for (;;) {
            if (deadline && Clock::now() >= *deadline) return;
            const size_t begin = next.fetch_add(batch, std::memory_order_relaxed);
            if (begin >= count) return;
            for (size_t i = begin; i < std::min(begin + batch, count); ++i) work(thread, i);
         }
      } catch (...) {
         failures[thread] = std::current_exception();
      }
   });
   for (const std::exception_ptr& failure : failures)
      if (failure) std::rethrow_exception(failure);
   return std::min(next.load(), count);
}

// Groups the items into categories and subcategories, every item into All as well, nearest first:
// the entries of each subcategory, then the subcategories by their nearest.
void group(List& list) {
   const auto count = static_cast<uint32_t>(list.items.size());
   // Each item's key by number, so that each is hashed once.
   std::unordered_map<std::string_view, uint32_t> ids;
   std::vector<uint32_t> keyOf(count);
   std::vector<double> distance(count);
   std::array<std::vector<uint32_t>, kCategoryCount> byCategory;
   for (uint32_t i = 0; i < count; ++i) {
      const Item& item = list.items[i];
      keyOf[i] = ids.try_emplace(item.key, static_cast<uint32_t>(ids.size())).first->second;
      distance[i] = distanceSquared(item.position, list.origin);
      if (item.category != All) byCategory[item.category].push_back(i);
   }

   std::vector<uint32_t> subcategoryOf(ids.size());
   std::vector<Subcategory*> subcategories;
   for (size_t category = 0; category < kCategoryCount; ++category) {
      auto& found = list.categories[category];
      found.clear();
      std::ranges::fill(subcategoryOf, UINT32_MAX);
      auto add = [&](uint32_t i) {
         uint32_t& place = subcategoryOf[keyOf[i]];
         if (place == UINT32_MAX) {
            place = static_cast<uint32_t>(found.size());
            found.push_back({list.items[i].key, {}});
         }
         found[place].items.push_back(i);
      };
      if (category == All)
         for (uint32_t i = 0; i < count; ++i) add(i);
      else
         for (uint32_t i : byCategory[category]) add(i);
      for (Subcategory& subcategory : found) subcategories.push_back(&subcategory);
   }

   // The biggest first, so that no thread is left with one at the end.
   std::ranges::sort(subcategories, std::greater{}, [](const Subcategory* s) { return s->items.size(); });
   parallelFor(threadsFor(count, 1 << 16), 0, subcategories.size(), 1, [&](unsigned, size_t index) {
      auto& items = subcategories[index]->items;
      std::vector<std::pair<double, uint32_t>> keyed;
      keyed.reserve(items.size());
      for (uint32_t item : items) keyed.emplace_back(distance[item], item);
      std::sort(keyed.begin(), keyed.end());
      for (size_t i = 0; i < keyed.size(); ++i) items[i] = keyed[i].second;
   });
   for (auto& found : list.categories)
      std::stable_sort(found.begin(), found.end(), [&](const Subcategory& a, const Subcategory& b) {
         return distance[a.items.front()] < distance[b.items.front()];
      });
}

// An entity met in the walk.
struct Found {
   std::byte* entity;
   const std::byte* prototype;
   Position position;
};

// Trees and resources, counted by cell rather than kept one by one. Touching cells of one group
// (all trees, or one resource prototype) make one forest or patch. Every cell lies in one chunk, as
// the cells' sides divide 32, so each chunk keeps a table of its cells of each group, and only
// neighbours across a chunk's edge are looked up by chunk.
class Cells {
public:
   explicit Cells(Position origin) : origin_(origin) {}

   // Starts chunk (`x`, `y`): the walk adds each chunk's entities together, all standing in it.
   void beginChunk(int32_t x, int32_t y) {
      chunkX_ = x;
      chunkY_ = y;
      firstHere_ = tables_.size();
   }

   // Puts `found` in its cell of `group`, cells being 2^shift tiles a side. One listed alone only
   // joins the cells around it: it is neither counted nor the forest's or patch's place.
   void add(const std::byte* group, int shift, const Found& found, bool counted) {
      const int32_t x = tileOf(found.position.x), y = tileOf(found.position.y);
      const int32_t side = 32 >> shift;
      size_t table = firstHere_;
      while (table < tables_.size() && tables_[table].group != group) ++table;
      if (table == tables_.size()) {
         tables_.push_back({chunkX_, chunkY_, group, side, static_cast<uint32_t>(slots_.size())});
         slots_.resize(slots_.size() + static_cast<size_t>(side * side), kNoCell);
      }
      uint32_t& index = slots_[tables_[table].offset + ((x & 31) >> shift) + ((y & 31) >> shift) * side];
      if (index == kNoCell) {
         index = static_cast<uint32_t>(cells_.size());
         cells_.push_back({group});
      }
      Cell& cell = cells_[index];
      if (!counted) return;
      ++cell.count;
      cell.box.add(x, y);
      const double distance = distanceSquared(found.position, origin_);
      if (nearer(distance, found.position, cell.nearest, cell.position)) {
         cell.nearest = distance;
         cell.position = found.position;
         cell.prototype = found.prototype;
      }
   }

   // A forest or patch: its group, how many it counted, and the one nearest the origin.
   struct Group {
      const std::byte* group;
      uint32_t count = 0;
      double nearest = INFINITY;
      Position position{};
      const std::byte* prototype = nullptr;
      TileBox box;
   };

   // The forests and patches of the cells of `parts`, each of which walked chunks of its own.
   static std::vector<Group> groups(std::span<const Cells* const> parts) {
      // Every part's cells, numbered one after another.
      std::vector<uint32_t> bases;
      uint32_t cellCount = 0;
      size_t tableCount = 0;
      for (const Cells* part : parts) {
         bases.push_back(cellCount);
         cellCount += static_cast<uint32_t>(part->cells_.size());
         tableCount += part->tables_.size();
      }
      struct Place {
         const Cells* part = nullptr;
         uint32_t base = 0;
         const Table* table = nullptr;
         uint32_t cell(int32_t x, int32_t y) const {
            return part->slots_[table->offset + static_cast<uint32_t>(x + y * table->side)];
         }
      };
      std::unordered_map<TableKey, Place, TableKeyHash> tables;
      tables.reserve(tableCount);
      for (size_t p = 0; p < parts.size(); ++p)
         for (const Table& table : parts[p]->tables_)
            tables.emplace(TableKey{table.x, table.y, table.group}, Place{parts[p], bases[p], &table});

      Sets sets;
      for (uint32_t i = 0; i < cellCount; ++i) sets.add();
      // Half the 8 neighbours: the other half joins from the far side.
      constexpr std::array<std::pair<int32_t, int32_t>, 4> kNeighbours{{{1, 0}, {1, 1}, {0, 1}, {-1, 1}}};
      for (size_t p = 0; p < parts.size(); ++p) {
         for (const Table& table : parts[p]->tables_) {
            const Place here{parts[p], bases[p], &table};
            // The same group's tables in the chunks beside, below and below beside, by (dx + 1) + 3 * dy.
            std::array<std::optional<Place>, 6> around;
            auto beside = [&](int32_t dx, int32_t dy) -> const Place& {
               std::optional<Place>& place = around[static_cast<size_t>(dx + 1 + 3 * dy)];
               if (!place) {
                  auto found = tables.find({table.x + dx, table.y + dy, table.group});
                  place = found == tables.end() ? Place{} : found->second;
               }
               return *place;
            };
            const int32_t side = table.side;
            for (int32_t y = 0; y < side; ++y)
               for (int32_t x = 0; x < side; ++x) {
                  const uint32_t cell = here.cell(x, y);
                  if (cell == kNoCell) continue;
                  for (auto [dx, dy] : kNeighbours) {
                     const int32_t nx = x + dx, ny = y + dy;
                     const int32_t chunkDx = nx < 0 ? -1 : nx >= side ? 1 : 0, chunkDy = ny >= side ? 1 : 0;
                     const Place& there = chunkDx || chunkDy ? beside(chunkDx, chunkDy) : here;
                     if (!there.part) continue;
                     const uint32_t other = there.cell(nx - chunkDx * side, ny - chunkDy * side);
                     if (other != kNoCell) sets.join(here.base + cell, there.base + other);
                  }
               }
         }
      }

      std::vector<uint32_t> groupOf(cellCount, UINT32_MAX);
      std::vector<Group> groups;
      for (size_t p = 0; p < parts.size(); ++p) {
         const auto& cells = parts[p]->cells_;
         for (uint32_t i = 0; i < cells.size(); ++i) {
            const Cell& cell = cells[i];
            const uint32_t root = sets.find(bases[p] + i);
            if (groupOf[root] == UINT32_MAX) {
               groupOf[root] = static_cast<uint32_t>(groups.size());
               groups.push_back({cell.group});
            }
            Group& group = groups[groupOf[root]];
            group.count += cell.count;
            if (cell.count == 0) continue;
            group.box.add(cell.box);
            if (nearer(cell.nearest, cell.position, group.nearest, group.position)) {
               group.nearest = cell.nearest;
               group.position = cell.position;
               group.prototype = cell.prototype;
            }
         }
      }
      std::erase_if(groups, [](const Group& group) { return group.count == 0; });
      // In an order of their own, not the cells'. Groups of two resources may share their nearest.
      std::sort(groups.begin(), groups.end(), [](const Group& a, const Group& b) {
         if (a.position != b.position) return nearer(a.nearest, a.position, b.nearest, b.position);
         return std::less<const void*>{}(a.group, b.group);
      });
      return groups;
   }

   size_t size() const { return cells_.size(); }

private:
   struct Cell {
      const std::byte* group;
      uint32_t count = 0;
      double nearest = INFINITY;
      Position position{};
      const std::byte* prototype = nullptr;
      TileBox box;
   };

   static constexpr uint32_t kNoCell = UINT32_MAX;

   // A chunk's cells of one group: `side` a side, by place in the chunk from `offset` in slots_, each
   // its index in cells_ or kNoCell.
   struct Table {
      int32_t x;
      int32_t y;
      const std::byte* group;
      int32_t side;
      uint32_t offset;
   };
   struct TableKey {
      int32_t x;
      int32_t y;
      const std::byte* group;
      bool operator==(const TableKey&) const = default;
   };
   struct TableKeyHash {
      size_t operator()(const TableKey& key) const {
         return std::hash<const void*>{}(key.group) ^
                std::hash<uint64_t>{}(packCell(key.x, key.y) * 0x9e3779b97f4a7c15);
      }
   };

   Position origin_;
   std::vector<Cell> cells_;
   std::vector<Table> tables_;
   std::vector<uint32_t> slots_;
   int32_t chunkX_ = 0;
   int32_t chunkY_ = 0;
   size_t firstHere_ = 0; // the current chunk's first table
};

// Builds items from what a refresh walked, and the entities to link: an entity item's `first` is its
// place in `entities`.
class Builder {
public:
   Builder(std::vector<Item>& items, std::vector<std::byte*>& entities) : items_(items), entities_(entities) {}

   void alone(const Found& found, const Rule& rule) {
      add({Kind::Entity, rule.category, found.prototype, found.position,
           subcategoryKey(found.entity, found.prototype, rule)},
          found);
   }

   // A tree, or a well of an infinite resource, near enough the origin to be listed by itself.
   void near(const Found& found, std::string key) {
      add({Kind::Entity, Resources, found.prototype, found.position, std::move(key)}, found);
   }

   // A resource whose prototype makes every resource a patch of its own.
   void patchOfOne(const Found& found) {
      Item item{Kind::Patch, Resources, found.prototype, found.position, std::string(prototypeName(found.prototype))};
      item.count = 1;
      item.box.add(tileOf(found.position.x), tileOf(found.position.y));
      items_.push_back(std::move(item));
   }

private:
   void add(Item item, const Found& found) {
      item.first = static_cast<uint32_t>(entities_.size());
      item.count = 1;
      entities_.push_back(found.entity);
      items_.push_back(std::move(item));
   }

   std::vector<Item>& items_;
   std::vector<std::byte*>& entities_;
};

// Prototype names by prototype, for work off the game's threads, which must not read the game.
using Names = std::unordered_map<const std::byte*, std::string>;

// Adds the forests (trees grouped under null) and patches of `cells` to `items`.
void addGroups(std::vector<Item>& items, std::span<const Cells* const> cells, const Names& names) {
   for (const Cells::Group& group : Cells::groups(cells)) {
      const bool forest = !group.group;
      Item item{forest ? Kind::Forest : Kind::Patch, Resources, forest ? group.prototype : group.group, group.position,
                forest ? "tree" : names.at(group.group)};
      item.count = group.count;
      item.box = group.box;
      items.push_back(std::move(item));
   }
}

// Bodies of water or ice: connected tiles of one class, 8 ways, chunk by chunk. Each chunk labels its
// own components; components touching across chunk borders are then joined.
class TileBodies {
public:
   TileBodies(TileClass tileClass, Position origin) : class_(tileClass), origin_(origin) {}

   void addChunk(const std::byte* chunk, int32_t cx, int32_t cy, const std::vector<uint8_t>& classes) {
      std::array<uint16_t, 1024> labels{};
      bool any = false;
      for (int32_t i = 0; i < 1024; ++i) {
         const uint16_t id = at<uint16_t>(chunk, layout.chunkTiles + static_cast<uint32_t>(i) * layout.tileSize);
         if (id < classes.size() && classes[id] == class_) {
            labels[i] = kUnlabelled;
            any = true;
         }
      }
      if (!any) return;

      Chunk entry{cx, cy, sets_.size(), {}};
      uint16_t next = 1;
      std::vector<int32_t> stack;
      for (int32_t start = 0; start < 1024; ++start) {
         if (labels[start] != kUnlabelled) continue;
         const uint16_t label = next++;
         const uint32_t node = sets_.add();
         stats_.push_back({});
         labels[start] = label;
         stack.push_back(start);
         while (!stack.empty()) {
            const int32_t tile = stack.back();
            stack.pop_back();
            const int32_t x = tile / 32, y = tile % 32;
            stats_[node].add(cx * 32 + x, cy * 32 + y, origin_);
            for (int32_t dx = -1; dx <= 1; ++dx)
               for (int32_t dy = -1; dy <= 1; ++dy) {
                  const int32_t nx = x + dx, ny = y + dy;
                  if (nx < 0 || nx >= 32 || ny < 0 || ny >= 32) continue;
                  const int32_t neighbour = nx * 32 + ny;
                  if (labels[neighbour] != kUnlabelled) continue;
                  labels[neighbour] = label;
                  stack.push_back(neighbour);
               }
         }
      }
      entry.labels = labels;
      index_.emplace(packCell(cx, cy), chunks_.size());
      chunks_.push_back(std::move(entry));
   }

   // Takes over the chunks another thread labelled, none of them this one's. Their components are
   // all apart still: only bodies() joins them.
   void absorb(TileBodies&& other) {
      const uint32_t base = sets_.size();
      for (uint32_t i = 0; i < other.sets_.size(); ++i) sets_.add();
      stats_.insert(stats_.end(), other.stats_.begin(), other.stats_.end());
      index_.reserve(index_.size() + other.chunks_.size());
      for (Chunk& chunk : other.chunks_) {
         chunk.base += base;
         index_.emplace(packCell(chunk.x, chunk.y), chunks_.size());
         chunks_.push_back(chunk);
      }
      other = TileBodies(class_, origin_);
   }

   // Each body, nearest first: its box, and its tile nearest the origin.
   struct Body {
      TileBox box;
      int32_t nearestX = 0;
      int32_t nearestY = 0;
   };
   std::vector<Body> bodies() {
      for (const Chunk& chunk : chunks_) {
         joinAcross(chunk, 1, 0, [](int32_t i) { return std::pair{31, i}; }, [](int32_t i) { return std::pair{0, i}; });
         joinAcross(chunk, 0, 1, [](int32_t i) { return std::pair{i, 31}; }, [](int32_t i) { return std::pair{i, 0}; });
         joinCorner(chunk, 1, 1, {31, 31}, {0, 0});
         joinCorner(chunk, -1, 1, {0, 31}, {31, 0});
      }
      std::unordered_map<uint32_t, Stats> roots;
      for (uint32_t node = 0; node < sets_.size(); ++node) roots[sets_.find(node)].add(stats_[node]);
      std::vector<Stats> found;
      found.reserve(roots.size());
      for (const auto& [root, stats] : roots) found.push_back(stats);
      std::sort(found.begin(), found.end(), [](const Stats& a, const Stats& b) {
         return nearer(a.nearest, a.position(), b.nearest, b.position());
      });
      std::vector<Body> result;
      result.reserve(found.size());
      for (const Stats& stats : found) result.push_back({stats.box, stats.nearestX, stats.nearestY});
      return result;
   }

private:
   static constexpr uint16_t kUnlabelled = 0xffff;

   struct Stats {
      TileBox box;
      double nearest = INFINITY;
      int32_t nearestX = 0;
      int32_t nearestY = 0;
      Position position() const { return tileCentre(nearestX, nearestY); }
      void add(int32_t x, int32_t y, Position origin) {
         box.add(x, y);
         const double distance = distanceSquared(tileCentre(x, y), origin);
         if (nearer(distance, tileCentre(x, y), nearest, position())) {
            nearest = distance;
            nearestX = x;
            nearestY = y;
         }
      }
      void add(const Stats& other) {
         box.add(other.box);
         if (nearer(other.nearest, other.position(), nearest, position())) {
            nearest = other.nearest;
            nearestX = other.nearestX;
            nearestY = other.nearestY;
         }
      }
   };

   struct Chunk {
      int32_t x;
      int32_t y;
      uint32_t base; // node of label 1
      std::array<uint16_t, 1024> labels;
      uint16_t at(std::pair<int32_t, int32_t> tile) const { return labels[tile.first * 32 + tile.second]; }
   };

   const Chunk* chunkAt(int32_t x, int32_t y) const {
      auto found = index_.find(packCell(x, y));
      return found == index_.end() ? nullptr : &chunks_[found->second];
   }

   void join(const Chunk& a, uint16_t labelA, const Chunk& b, uint16_t labelB) {
      if (labelA && labelB) sets_.join(a.base + labelA - 1, b.base + labelB - 1);
   }

   // Joins along the border with the chunk at (dx, dy): edge tile i of this chunk touches edge tiles
   // i - 1 to i + 1 of the other.
   template <class Mine, class Theirs>
   void joinAcross(const Chunk& chunk, int32_t dx, int32_t dy, Mine mine, Theirs theirs) {
      const Chunk* other = chunkAt(chunk.x + dx, chunk.y + dy);
      if (!other) return;
      for (int32_t i = 0; i < 32; ++i) {
         const uint16_t label = chunk.at(mine(i));
         if (!label) continue;
         for (int32_t j = std::max(0, i - 1); j <= std::min(31, i + 1); ++j)
            join(chunk, label, *other, other->at(theirs(j)));
      }
   }

   void joinCorner(const Chunk& chunk, int32_t dx, int32_t dy, std::pair<int32_t, int32_t> mine,
                   std::pair<int32_t, int32_t> theirs) {
      if (const Chunk* other = chunkAt(chunk.x + dx, chunk.y + dy))
         join(chunk, chunk.at(mine), *other, other->at(theirs));
   }

   TileClass class_;
   Position origin_{};
   Sets sets_;
   std::vector<Stats> stats_;
   std::vector<Chunk> chunks_;
   std::unordered_map<uint64_t, size_t> index_;
};

// Where the scanner keys stand, as entrypoint.lua's scanner_cursor: unset parts start at the first.
struct Cursor {
   std::optional<size_t> category;
   std::optional<size_t> subcategory;
   std::optional<size_t> entry;
};

// A move a scanner key made, kept until the mod asks what to say for it.
struct Move {
   std::optional<Position> target; // the position the key's input action carries
   Entry entry;
};
constexpr size_t kMovesKept = 32;

enum class Action { SubcategoryBack, SubcategoryNext, EntryBack, EntryNext, CategoryBack, CategoryNext, Repeat };

struct ScannerKey {
   const char* input; // the mod's custom input
   Action action;
};
constexpr ScannerKey kKeys[] = {
   {"fa-pageup", Action::SubcategoryBack},
   {"fa-pagedown", Action::SubcategoryNext},
   {"fa-s-pageup", Action::EntryBack},
   {"fa-s-pagedown", Action::EntryNext},
   {"fa-c-pageup", Action::CategoryBack},
   {"fa-c-pagedown", Action::CategoryNext},
   {"fa-home", Action::Repeat},
};

std::mutex g_mutex;
List g_list;
Cursor g_cursor;
std::deque<Move> g_moves;
std::optional<Category> g_category;
bool g_modUiOpen = false;
// The LuaSurface the player is on, as of the last tick; 0 before the first.
uint32_t g_surface = 0;

// The world the list was made in, while it is still there.
struct World {
   const std::byte* surface = nullptr;
   explicit operator bool() const { return surface; }
};

// A list of a surface the player left is not used while that of the player's is made.
World worldOf(const List& list) {
   const std::byte* game = currentGame();
   if (!game || game != list.game) return {};
   if (g_surface && list.surfaceIndex != g_surface) return {};
   const std::byte* player = at<const std::byte*>(game, layout.gameLocalPlayer);
   if (!player) return {};
   const std::byte* map = at<const std::byte*>(player, layout.playerMap);
   if (!map) return {};
   return {surfaceAt(map, list.surfaceIndex)};
}

// Entity `index` of the list's links, when it is still there on the list's surface.
const std::byte* liveEntity(const World& world, uint32_t index) {
   const std::byte* entity = g_list.links.entity(index);
   return entity && at<const std::byte*>(entity, layout.entitySurface) == world.surface ? entity : nullptr;
}

// The first of an item's entities still there, nearest first as they were listed.
const std::byte* firstLive(const World& world, const Item& item) {
   for (uint32_t i = item.first; i < item.first + item.count; ++i)
      if (const std::byte* entity = liveEntity(world, i)) return entity;
   return nullptr;
}

// The tree of a forest, or resource of a patch, nearest the list's origin in `box`, if any.
const std::byte* nearestMember(const World& world, const Item& item, const TileBox& box) {
   const std::byte* nearest = nullptr;
   double nearestDistance = INFINITY;
   forEachEntity(world.surface, {box.left >> 1, box.top >> 1}, {box.right >> 1, box.bottom >> 1},
                 [&](const std::byte* entity) {
                    const auto* prototype = at<const std::byte*>(entity, layout.entityPrototypeOf);
                    if (item.kind == Kind::Patch ? prototype != item.prototype
                                                 : std::string_view(prototypeType(prototype)) != "tree")
                       return;
                    const Position position = at<Position>(entity, layout.entityPosition);
                    const double distance = distanceSquared(position, g_list.origin);
                    if (distance < nearestDistance) {
                       nearestDistance = distance;
                       nearest = entity;
                    }
                 });
   return nearest;
}

// The forest's tree or patch's resource where the entry stands, else the nearest one left in it.
const std::byte* memberAt(const World& world, const Item& item) {
   const int32_t x = tileOf(item.position.x), y = tileOf(item.position.y);
   TileBox here;
   here.add(x, y);
   if (const std::byte* member = nearestMember(world, item, here)) return member;
   return nearestMember(world, item, item.box);
}

// Whether entry `index` is still there, bringing its position up to date. Reads the world, so only
// while it stands still.
bool validate(const World& world, uint32_t index) {
   Item& item = g_list.items[index];
   switch (item.kind) {
   case Kind::Entity:
      if (const std::byte* entity = firstLive(world, item)) {
         item.position = at<Position>(entity, layout.entityPosition);
         return true;
      }
      return false;
   case Kind::Forest:
   case Kind::Patch:
      if (const std::byte* member = memberAt(world, item)) {
         item.position = at<Position>(member, layout.entityPosition);
         return true;
      }
      return false;
   case Kind::Water:
   case Kind::Ice: {
      // Landfill may have covered the tile.
      const auto id = tileAt(world.surface, tileOf(item.position.x), tileOf(item.position.y));
      const TileClass wanted = item.kind == Kind::Water ? WaterTile : IceTile;
      return id && *id < g_list.tileClasses.size() && g_list.tileClasses[*id] == wanted;
   }
   case Kind::Extra:
      // Only the mod can tell whether its pin or tag is still there; it says so when it says it.
      return true;
   }
   return false;
}

enum class Result { Moved, AtBeginning, AtEnd };

Result edge(int direction) { return direction < 0 ? Result::AtBeginning : Result::AtEnd; }

// Drops the invalid entries ahead of the first valid one of `subcategory`; whether one is left.
bool firstValid(const World& world, Subcategory& subcategory) {
   auto& items = subcategory.items;
   while (!items.empty()) {
      if (validate(world, items.front())) return true;
      items.erase(items.begin());
   }
   return false;
}

Result moveCategory(const World& world, int direction) {
   if (!g_cursor.category) {
      g_cursor = {All, {}, {}};
      return Result::Moved;
   }
   for (ptrdiff_t index = static_cast<ptrdiff_t>(*g_cursor.category) + direction; index >= 0 && index < kCategoryCount;
        index += direction) {
      auto& subcategories = g_list.categories[index];
      while (!subcategories.empty()) {
         if (firstValid(world, subcategories.front())) {
            g_cursor = {static_cast<size_t>(index), {}, {}};
            return Result::Moved;
         }
         subcategories.erase(subcategories.begin());
      }
   }
   return edge(direction);
}

Result moveSubcategory(const World& world, int direction) {
   if (!g_cursor.category) g_cursor.category = All;
   auto& subcategories = g_list.categories[*g_cursor.category];
   if (subcategories.empty()) return edge(direction);
   ptrdiff_t index = g_cursor.subcategory ? static_cast<ptrdiff_t>(*g_cursor.subcategory) + direction : 0;
   for (;;) {
      if (index < 0 || index >= static_cast<ptrdiff_t>(subcategories.size())) return edge(direction);
      if (firstValid(world, subcategories[index])) {
         g_cursor.subcategory = static_cast<size_t>(index);
         g_cursor.entry.reset();
         return Result::Moved;
      }
      subcategories.erase(subcategories.begin() + index);
      if (direction < 0) --index;
   }
}

Result moveEntry(const World& world, int direction) {
   if (!g_cursor.category) return edge(direction);
   auto& subcategories = g_list.categories[*g_cursor.category];
   size_t sub = g_cursor.subcategory.value_or(0);
   g_cursor.subcategory = sub;
   if (sub >= subcategories.size()) return edge(direction);
   auto& items = subcategories[sub].items;
   ptrdiff_t start = g_cursor.entry ? static_cast<ptrdiff_t>(*g_cursor.entry) : -1;
   for (ptrdiff_t index = start + direction; index >= 0 && index < static_cast<ptrdiff_t>(items.size());
        index += direction) {
      if (validate(world, items[index])) {
         g_cursor.entry = static_cast<size_t>(index);
         return Result::Moved;
      }
   }
   return edge(direction);
}

// What the cursor is on, dropping what is gone on the way, as entrypoint.lua announce_cursor_pos.
std::optional<std::pair<uint32_t, uint32_t>> current(const World& world) {
   if (!g_cursor.category) g_cursor.category = All;
   size_t sub = g_cursor.subcategory.value_or(0);
   size_t entry = g_cursor.entry.value_or(0);
   auto& subcategories = g_list.categories[*g_cursor.category];
   while (!subcategories.empty()) {
      sub = std::min(sub, subcategories.size() - 1);
      auto& items = subcategories[sub].items;
      while (!items.empty()) {
         entry = std::min(entry, items.size() - 1);
         if (validate(world, items[entry])) {
            g_cursor.subcategory = sub;
            g_cursor.entry = entry;
            return std::pair{static_cast<uint32_t>(entry), static_cast<uint32_t>(items.size())};
         }
         items.erase(items.begin() + entry);
      }
      subcategories.erase(subcategories.begin() + sub);
   }
   g_cursor.subcategory = sub;
   g_cursor.entry = entry;
   return std::nullopt;
}

// Fills in what the mod says of a valid item.
void describe(const World& world, const Item& item, Entry& entry) {
   entry.kind = std::string(kindName(item.kind));
   switch (item.kind) {
   case Kind::Entity: entry.prototype = std::string(prototypeName(item.prototype)); break;
   case Kind::Forest: {
      if (item.count == 1) {
         // A forest of one tree is said as a tree.
         const std::byte* tree = memberAt(world, item);
         entry.kind = std::string(kindName(Kind::Entity));
         entry.prototype = std::string(prototypeName(at<const std::byte*>(tree, layout.entityPrototypeOf)));
      }
      entry.trees = item.count;
      break;
   }
   case Kind::Patch: {
      const std::byte* resource = memberAt(world, item);
      PatchFinder finder(g_list.force);
      finder.find(resource);
      entry.text = finder.label(resource);
      entry.prototype = std::string(prototypeName(item.prototype));
      break;
   }
   case Kind::Water:
   case Kind::Ice:
      entry.width = item.box.width();
      entry.height = item.box.height();
      break;
   case Kind::Extra:
      entry.extra = item.first + 1;
      entry.generation = g_list.generation;
      break;
   }
}

std::optional<Action> actionFor(const input::KeyEvent& key) {
   for (const ScannerKey& scannerKey : kKeys)
      for (const bindings::Key& bound : bindings::customInput(scannerKey.input))
         if (bound.key == key.key && bound.shift == key.shift && bound.ctrl == key.ctrl && bound.alt == key.alt)
            return scannerKey.action;
   return std::nullopt;
}

void remember(Move move) {
   if (g_moves.size() >= kMovesKept) g_moves.pop_front();
   g_moves.push_back(std::move(move));
}

// Carries out a scanner key; the position its input action is to carry, if any.
std::optional<Position> act(Action action) {
   World world = worldOf(g_list);
   if (!world) {
      g_list = {};
      g_cursor = {};
   }
   if (action == Action::CategoryBack || action == Action::CategoryNext) {
      Result result = world ? moveCategory(world, action == Action::CategoryBack ? -1 : 1) : Result::Moved;
      if (!g_cursor.category) g_cursor.category = All;
      g_category = Category{std::string(kCategoryKeys[*g_cursor.category]), result != Result::Moved};
      return std::nullopt;
   }

   Result result = Result::Moved;
   if (world) {
      switch (action) {
      case Action::SubcategoryBack: result = moveSubcategory(world, -1); break;
      case Action::SubcategoryNext: result = moveSubcategory(world, 1); break;
      case Action::EntryBack: result = moveEntry(world, -1); break;
      case Action::EntryNext: result = moveEntry(world, 1); break;
      default: break;
      }
   }
   if (!g_cursor.category) g_cursor.category = All;

   Move move;
   move.entry.category = std::string(kCategoryKeys[*g_cursor.category]);
   move.entry.edge = result != Result::Moved;
   std::optional<std::pair<uint32_t, uint32_t>> place = world ? current(world) : std::nullopt;
   if (!place) {
      move.entry.empty = true;
      if (auto cursor = world::cursorPosition()) move.target = Position{cursor->x, cursor->y};
   } else {
      const auto& subcategory = g_list.categories[*g_cursor.category][*g_cursor.subcategory];
      const Item& item = g_list.items[subcategory.items[*g_cursor.entry]];
      move.target = item.kind == Kind::Water || item.kind == Kind::Ice
                       ? tileCentre(tileOf(item.position.x), tileOf(item.position.y))
                       : item.position;
      move.entry.index = place->first + 1;
      move.entry.count = place->second;
      describe(world, item, move.entry);
      move.entry.x = tiles(move.target->x);
      move.entry.y = tiles(move.target->y);
      move.entry.originX = tiles(g_list.origin.x);
      move.entry.originY = tiles(g_list.origin.y);
   }
   std::optional<Position> target = move.target;
   remember(std::move(move));
   return target;
}

// TileClass by tile ID, from the mod's names of water and ice tiles.
std::vector<uint8_t> tileClasses(const Refresh& request) {
   const auto& prototypes = *reinterpret_cast<const MsvcVector<const std::byte*>*>(layout.tilePrototypes);
   std::vector<uint8_t> classes(static_cast<size_t>(prototypes.last - prototypes.first), NoClass);
   for (size_t id = 0; id < classes.size(); ++id) {
      const std::byte* prototype = prototypes.first[id];
      if (!prototype) continue;
      const std::string_view name = prototypeName(prototype);
      if (std::find(request.water.begin(), request.water.end(), name) != request.water.end())
         classes[id] = WaterTile;
      else if (std::find(request.ice.begin(), request.ice.end(), name) != request.ice.end())
         classes[id] = IceTile;
   }
   return classes;
}

// Milliseconds since the last lap, for the refresh's log line.
class Stopwatch {
public:
   double lap() {
      const auto now = std::chrono::steady_clock::now();
      const double ms = std::chrono::duration<double, std::milli>(now - last_).count();
      last_ = now;
      return ms;
   }

private:
   std::chrono::steady_clock::time_point last_ = std::chrono::steady_clock::now();
};

constexpr double kTreeZoomSquared = kForestZoomDistance * kForestZoomDistance;
constexpr double kWellZoomSquared = kInfiniteResourceZoomDistance * kInfiniteResourceZoomDistance;

// Whether an entry at `position` is listed: in `direction` from `origin`, when there is one.
bool inDirection(Position position, Position origin, std::optional<int> direction) {
   return !direction || directionBiased(position, origin) == *direction;
}

using ChunkAtFunction = const std::byte* (*)(const void* surface, const Position* chunkPosition);

// What a refresh lists, read by every thread of its walk. Its pointers hold for one tick only, so a
// refresh spread over ticks finds them again each tick.
struct Scan {
   const std::byte* surface;
   uint32_t surfaceIndex; // the game's SurfaceIndex, one less than the LuaSurface's
   const std::byte* force;
   Position origin;
   std::optional<int> direction;
   const std::vector<uint8_t>& tileClasses;

   bool wanted(Position position) const { return inDirection(position, origin, direction); }

   bool charted(Position position) const {
      return reinterpret_cast<ChartedFunction>(layout.forceIsChunkCharted)(force, surfaceIndex, &position);
   }

   // The chunk at `chunkPosition` when it is there and charted.
   const std::byte* chunkAt(Position chunkPosition) const {
      const std::byte* chunk = reinterpret_cast<ChunkAtFunction>(layout.surfaceChunkAt)(surface, &chunkPosition);
      if (!chunk) return nullptr;
      return charted({chunkPosition.x * 32 * 256 + 16 * 256, chunkPosition.y * 32 * 256 + 16 * 256}) ? chunk : nullptr;
   }
};

// One thread's share of a refresh's walk: whole chunks, handed out a batch at a time.
struct Walk {
   explicit Walk(Position origin) : cells(origin), water(WaterTile, origin), ice(IceTile, origin) {}

   std::vector<Item> items;
   // The entities of entity items not yet linked, by Item::first; linked items' `first` is their
   // place in the refresh's Links.
   std::vector<std::byte*> entities;
   size_t linkedItems = 0;
   // The items of the chunks walked, by the chunk's place in the walk: up to `end` in items.
   struct Range {
      size_t chunk;
      size_t end;
   };
   std::vector<Range> ranges;
   Cells cells;
   TileBodies water;
   TileBodies ice;
   std::unordered_map<const std::byte*, Rule> rules;
   const std::byte* lastPrototype = nullptr;
   const Rule* lastRule = nullptr;
   size_t trees = 0;
   size_t resources = 0;
   size_t chunks = 0;

   const Rule& ruleOf(const std::byte* prototype) {
      if (prototype != lastPrototype) {
         auto [found, added] = rules.try_emplace(prototype);
         if (added) found->second = ruleFor(prototype, prototypeType(prototype), prototypeName(prototype));
         lastPrototype = prototype;
         lastRule = &found->second;
      }
      return *lastRule;
   }
};

void walkChunk(const Scan& scan, Walk& walk, const std::byte* chunk, size_t place) {
   const Position chunkPosition = at<Position>(chunk, layout.chunkPosition);
   const Position first{chunkPosition.x * 16, chunkPosition.y * 16};
   const Position last{first.x + 15, first.y + 15};
   Builder builder(walk.items, walk.entities);
   walk.cells.beginChunk(chunkPosition.x, chunkPosition.y);
   forEachEntity(scan.surface, first, last, [&](const std::byte* entity) {
      // The iterator also gives entities standing just past the chunk's edge, which their own
      // chunk gives again: each chunk keeps those standing in it.
      const Position position = at<Position>(entity, layout.entityPosition);
      if ((tileOf(position.x) >> 5) != chunkPosition.x || (tileOf(position.y) >> 5) != chunkPosition.y) return;
      if (at<uint16_t>(entity, layout.entityUsageBits) & kNotListedBits) return;
      const std::byte* prototype = at<const std::byte*>(entity, layout.entityPrototypeOf);
      const Rule& rule = walk.ruleOf(prototype);
      if (rule.listing == Listing::No) return;
      if (!scan.wanted(position)) return;
      const Found found{const_cast<std::byte*>(entity), prototype, position};
      switch (rule.listing) {
      case Listing::Alone: builder.alone(found, rule); break;
      case Listing::Tree: {
         ++walk.trees;
         // Trees near the origin are listed alone, but still join the forest around them.
         const bool near = distanceSquared(position, scan.origin) < kTreeZoomSquared;
         if (near) builder.near(found, "tree");
         walk.cells.add(nullptr, kForestCellShift, found, !near);
         break;
      }
      case Listing::Resource: {
         ++walk.resources;
         if (rule.cellShift < 0) {
            builder.patchOfOne(found);
            break;
         }
         // So are the wells of an infinite resource.
         const bool near = rule.infinite && distanceSquared(position, scan.origin) < kWellZoomSquared;
         if (near) builder.near(found, std::string(prototypeName(prototype)));
         walk.cells.add(prototype, rule.cellShift, found, !near);
         break;
      }
      case Listing::No: break;
      }
   });
   if (walk.items.size() != (walk.ranges.empty() ? 0 : walk.ranges.back().end))
      walk.ranges.push_back({place, walk.items.size()});
   walk.water.addChunk(chunk, chunkPosition.x, chunkPosition.y, scan.tileClasses);
   walk.ice.addChunk(chunk, chunkPosition.x, chunkPosition.y, scan.tileClasses);
   ++walk.chunks;
}

// Chunks a thread takes at a time: few enough that the threads finish together, and that a slice
// stops soon after its time is up.
constexpr size_t kChunksPerBatch = 16;
// How long each tick works for an automatic refresh, and how long after one the next starts: a
// second, or longer where a refresh takes long, so that they take at most this share of the time the
// game's update thread runs.
constexpr auto kSliceBudget = std::chrono::milliseconds(2);
constexpr auto kCycleInterval = std::chrono::seconds(1);
constexpr double kCycleShare = 0.05;

// Puts the walks' items into `items` in the order of their chunks, as one thread would have listed
// them.
void mergeItems(std::deque<Walk>& walks, std::vector<Item>& items) {
   struct Piece {
      size_t chunk;
      Walk* walk;
      size_t begin;
      size_t end;
   };
   std::vector<Piece> pieces;
   size_t total = 0;
   for (Walk& walk : walks) {
      size_t begin = 0;
      for (const Walk::Range& range : walk.ranges) {
         pieces.push_back({range.chunk, &walk, begin, range.end});
         begin = range.end;
      }
      total += walk.items.size();
   }
   std::sort(pieces.begin(), pieces.end(), [](const Piece& a, const Piece& b) { return a.chunk < b.chunk; });
   items.reserve(items.size() + total);
   for (const Piece& piece : pieces)
      for (size_t i = piece.begin; i < piece.end; ++i) items.push_back(std::move(piece.walk->items[i]));
}

// The local player's surface and force this tick, while `game` is still the one running.
struct Place {
   const std::byte* surface;
   uint32_t surfaceIndex; // the game's SurfaceIndex
   const std::byte* force;
};
std::optional<Place> placeOf(const std::byte* game, int playerIndex, uint32_t luaSurfaceIndex) {
   if (currentGame() != game) return std::nullopt;
   const std::byte* player = localPlayer(game, playerIndex);
   if (!player) return std::nullopt;
   const std::byte* map = at<const std::byte*>(player, layout.playerMap);
   const std::byte* surface = map ? surfaceAt(map, luaSurfaceIndex) : nullptr;
   if (!surface) return std::nullopt;
   return Place{surface, at<uint32_t>(surface, layout.surfaceIndex), forceOf(player)};
}

// A list being made: its chunks walked nearest first, a slice at a time while the world stands still,
// each slice linking the entities it found before the world moves on; then put together on a thread
// of its own, as that reads nothing of the game.
struct Job {
   Job(const std::byte* game, const Refresh& request, Position origin, std::optional<int> direction)
      : game(game), playerIndex(request.playerIndex), surfaceIndex(request.surfaceIndex), origin(origin),
        direction(direction), generation(request.generation), tileClasses(scanner::tileClasses(request)), links(game) {}
   Job(const Job&) = delete;
   Job& operator=(const Job&) = delete;

   const std::byte* game;
   int playerIndex;
   uint32_t surfaceIndex; // LuaSurface::index
   Position origin;
   std::optional<int> direction;
   uint32_t generation;
   std::vector<uint8_t> tileClasses;
   std::vector<Item> extras;    // the mod's, where they are listed
   std::vector<Position> order; // the chunks' positions, nearest first
   size_t next = 0;             // the first in `order` not walked
   std::deque<Walk> walks;
   Links links;
   Names names; // of the resources walked, for putting together
   size_t slices = 0;
   double startMs = 0;
   double walkMs = 0;
   double assembleMs = 0; // off the game's threads
   double installMs = 0;

   // How long it held the game's update thread.
   double heldMs() const { return startMs + walkMs + installMs; }

   // Put together, once `assembled`; null if that failed.
   std::optional<List> list;
   std::atomic<bool> assembled = false;
   std::jthread assembler; // last, so that it ends before what it reads goes
};

// Starts a list of the request's surface from `origin`, or null when there is no such surface.
std::unique_ptr<Job> startJob(const Refresh& request, Position origin, std::optional<int> direction) {
   const std::byte* game = currentGame();
   const std::optional<Place> place = placeOf(game, request.playerIndex, request.surfaceIndex);
   if (!place) {
      log::error("Scanner: no surface {} to list", request.surfaceIndex);
      return nullptr;
   }
   auto job = std::make_unique<Job>(game, request, origin, direction);
   const auto& chunks = at<MsvcVector<const std::byte*>>(place->surface, layout.surfaceChunks);
   std::vector<std::pair<double, Position>> keyed;
   keyed.reserve(static_cast<size_t>(chunks.last - chunks.first));
   for (const std::byte* const* chunk = chunks.first; chunk < chunks.last; ++chunk) {
      if (!*chunk) continue;
      const Position position = at<Position>(*chunk, layout.chunkPosition);
      keyed.emplace_back(distanceSquared(tileCentre(position.x * 32 + 15, position.y * 32 + 15), origin), position);
   }
   std::sort(keyed.begin(), keyed.end(),
             [](const auto& a, const auto& b) { return nearer(a.first, a.second, b.first, b.second); });
   job->order.reserve(keyed.size());
   for (const auto& [distance, position] : keyed) job->order.push_back(position);
   for (unsigned i = threadsFor(job->order.size(), kChunksPerBatch); i > 0; --i) job->walks.emplace_back(origin);

   const Scan scan{place->surface, place->surfaceIndex, place->force, origin, direction, job->tileClasses};
   for (size_t i = 0; i < request.extras.size(); ++i) {
      const Extra& extra = request.extras[i];
      const std::optional<Cat> category = categoryByKey(extra.category);
      if (!category) {
         log::error("Scanner: no category {} for an entry of the mod's", extra.category);
         continue;
      }
      const Position position{fixedPoint(extra.x), fixedPoint(extra.y)};
      if (!scan.wanted(position) || !scan.charted(position)) continue;
      Item item{Kind::Extra, *category, nullptr, position, extra.key};
      item.first = static_cast<uint32_t>(i);
      job->extras.push_back(std::move(item));
   }
   return job;
}

enum class Walked { Some, All, Gone };

// Walks the job's next chunks, until `deadline` if there is one, and links what they hold. Only
// while the world stands still.
Walked walkSlice(Job& job, std::optional<Clock::time_point> deadline) {
   const std::optional<Place> place = placeOf(job.game, job.playerIndex, job.surfaceIndex);
   if (!place) return Walked::Gone;
   Stopwatch stopwatch;
   const Scan scan{place->surface, place->surfaceIndex, place->force, job.origin, job.direction, job.tileClasses};
   job.next = parallelFor(
      static_cast<unsigned>(job.walks.size()), job.next, job.order.size(), kChunksPerBatch,
      [&](unsigned thread, size_t i) {
         if (const std::byte* chunk = scan.chunkAt(job.order[i])) walkChunk(scan, job.walks[thread], chunk, i);
      },
      deadline);
   // The entities found are surely there only until the world moves on.
   for (Walk& walk : job.walks) {
      for (size_t i = walk.linkedItems; i < walk.items.size(); ++i) {
         Item& item = walk.items[i];
         if (item.kind == Kind::Entity) item.first = job.links.add(walk.entities[item.first]);
      }
      walk.linkedItems = walk.items.size();
      walk.entities.clear();
   }
   ++job.slices;
   job.walkMs += stopwatch.lap();
   if (job.next < job.order.size()) return Walked::Some;
   for (const Walk& walk : job.walks)
      for (const auto& [prototype, rule] : walk.rules)
         if (rule.listing == Listing::Resource) job.names.try_emplace(prototype, prototypeName(prototype));
   return Walked::All;
}

// Puts the walked job's list together. Reads nothing of the game, so it may run on any thread.
List assemble(Job& job) {
   List list;
   list.game = job.game;
   list.surfaceIndex = job.surfaceIndex;
   list.origin = job.origin;
   list.direction = job.direction;
   list.generation = job.generation;
   mergeItems(job.walks, list.items);
   std::vector<const Cells*> cells;
   Walk& merged = job.walks.front();
   for (Walk& walk : job.walks) {
      cells.push_back(&walk.cells);
      if (&walk == &merged) continue;
      merged.water.absorb(std::move(walk.water));
      merged.ice.absorb(std::move(walk.ice));
   }
   addGroups(list.items, cells, job.names);
   for (auto [bodies, kind, category, key] : {std::tuple{&merged.water, Kind::Water, Resources, "water"},
                                              std::tuple{&merged.ice, Kind::Ice, Terrain, "iceberg"}}) {
      for (const TileBodies::Body& body : bodies->bodies()) {
         const Position nearest = tileCentre(body.nearestX, body.nearestY);
         if (!inDirection(nearest, job.origin, job.direction)) continue;
         Item item{kind, category, nullptr, nearest, key};
         item.box = body.box;
         list.items.push_back(std::move(item));
      }
   }
   list.items.insert(list.items.end(), job.extras.begin(), job.extras.end());
   group(list);
   list.tileClasses = std::move(job.tileClasses);
   return list;
}

// Puts the job's list together off the game's threads; Job::assembled says when it is done.
void assembleApart(Job& job) {
   job.assembler = std::jthread([&job] {
      Stopwatch stopwatch;
      try {
         job.list = assemble(job);
      } catch (const std::exception& error) {
         log::error("Scanner: putting a list together failed: {}", error.what());
      }
      job.assembleMs = stopwatch.lap();
      job.assembled.store(true, std::memory_order_release);
   });
}

// Whether `a` of the list now and `b` of `fresh`, both of one subcategory, stand for the same thing:
// the same entity; a forest, patch or body whose box holds where the other was; the same pin or tag.
bool same(const Item& a, const List& fresh, const Item& b) {
   if (a.kind != b.kind) return false;
   switch (a.kind) {
   case Kind::Entity: {
      const std::byte* entity = g_list.links.entity(a.first);
      return entity && entity == fresh.links.entity(b.first);
   }
   case Kind::Forest:
   case Kind::Patch:
   case Kind::Water:
   case Kind::Ice: {
      const int32_t x = tileOf(a.position.x), y = tileOf(a.position.y);
      return x >= b.box.left && x <= b.box.right && y >= b.box.top && y <= b.box.bottom;
   }
   case Kind::Extra: return a.key == b.key && a.position == b.position;
   }
   return false;
}

// Where the cursor stands in `fresh` to stay on what it is on now: the same entry if `fresh` has it,
// else the first of the same subcategory, else the same category.
Cursor carried(const List& fresh) {
   if (!g_cursor.category) return {};
   const size_t category = *g_cursor.category;
   Cursor cursor{category, {}, {}};
   const auto& before = g_list.categories[category];
   if (!g_cursor.subcategory || *g_cursor.subcategory >= before.size()) return cursor;
   const Subcategory& subcategory = before[*g_cursor.subcategory];
   const auto& after = fresh.categories[category];
   const auto found = std::ranges::find(after, subcategory.key, &Subcategory::key);
   if (found == after.end()) return cursor;
   cursor.subcategory = static_cast<size_t>(found - after.begin());
   if (!g_cursor.entry || *g_cursor.entry >= subcategory.items.size()) return cursor;
   const Item& item = g_list.items[subcategory.items[*g_cursor.entry]];
   for (size_t i = 0; i < found->items.size(); ++i)
      if (same(item, fresh, fresh.items[found->items[i]])) {
         cursor.entry = i;
         break;
      }
   return cursor;
}

// The links of lists put out of use, dropped a slice at a time. Only on the game's update thread.
std::deque<Links> g_retired;

// Drops retired links until `deadline`. Only while the world stands still.
void dropRetired(Clock::time_point deadline) {
   while (!g_retired.empty() && g_retired.front().releaseUntil(deadline)) g_retired.pop_front();
}

// Puts the job's list in place of the scanner's. `keep`: the cursor stays on what it is on;
// otherwise only its category stays, as the list was asked for anew. Only while the world stands
// still. The old list's links are dropped over the next ticks, and the rest of it on a thread of its
// own: a big list takes a while to go.
void install(Job& job, bool keep) {
   Stopwatch stopwatch;
   List& list = *job.list;
   list.links = std::move(job.links);
   const std::byte* player = localPlayer(job.game, job.playerIndex);
   list.force = player ? forceOf(player) : nullptr;
   {
      std::scoped_lock lock(g_mutex);
      const Cursor cursor = keep ? carried(list) : Cursor{g_cursor.category.value_or(All), {}, {}};
      List old = std::exchange(g_list, std::move(list));
      g_cursor = cursor;
      g_retired.push_back(std::move(old.links));
      std::thread([old = std::move(old)] {}).detach();
   }
   job.installMs = stopwatch.lap();
}

// The automatic refresh under way, if any. Only on the game's update thread, from Lua.
std::unique_ptr<Job> g_job;
// When the next automatic refresh starts while the list is of the player's surface, and when one is
// tried again while it is not.
Clock::time_point g_nextCycle;
Clock::time_point g_nextTry;

// Logs what a refresh found and how long it took.
void report(const Job& job, const char* how) {
   size_t trees = 0, resources = 0, chunks = 0, cells = 0;
   for (const Walk& walk : job.walks) {
      trees += walk.trees;
      resources += walk.resources;
      chunks += walk.chunks;
      cells += walk.cells.size();
   }
   log::info("Scanner: {} list of {} entries ({} trees, {} resources in {} cells) on surface {}: started in {:.1f} ms, "
             "{} chunks walked by {} threads in {} slices, {:.1f} ms; put together in {:.1f} ms, put in place in "
             "{:.1f} ms",
             how, g_list.items.size(), trees, resources, cells, job.surfaceIndex, job.startMs, chunks, job.walks.size(),
             job.slices, job.walkMs, job.assembleMs, job.installMs);
}

// When the next automatic refresh starts after `job`.
Clock::time_point nextCycleAfter(const Job& job) {
   const std::chrono::duration<double, std::milli> spacing(job.heldMs() / kCycleShare);
   return Clock::now() +
          std::max<Clock::duration>(kCycleInterval, std::chrono::duration_cast<Clock::duration>(spacing));
}

} // namespace

void refresh(const Refresh& request) {
   if (!localPlayer(currentGame(), request.playerIndex)) return;
   // This one takes the place of any under way.
   g_job.reset();
   Position origin{fixedPoint(request.x), fixedPoint(request.y)};
   std::optional<int> direction = request.direction;
   if (request.automatic) {
      // An automatic refresh keeps the list's direction, and in remote view its origin, which the
      // camera leaves as it follows the scanner.
      std::scoped_lock lock(g_mutex);
      if (g_list.game == currentGame() && g_list.surfaceIndex == request.surfaceIndex) {
         direction = g_list.direction;
         if (request.keepOrigin) origin = g_list.origin;
      }
   }
   Stopwatch stopwatch;
   std::unique_ptr<Job> job = startJob(request, origin, direction);
   if (!job) return;
   job->startMs = stopwatch.lap();
   if (request.automatic) {
      g_job = std::move(job);
      return;
   }

   if (walkSlice(*job, std::nullopt) != Walked::All) return;
   stopwatch.lap();
   job->list = assemble(*job);
   job->assembleMs = stopwatch.lap();
   install(*job, false);
   g_nextCycle = nextCycleAfter(*job);
   report(*job, "asked");
}

bool tick(int playerIndex, uint32_t surfaceIndex) {
   const std::byte* game = currentGame();
   if (!localPlayer(game, playerIndex)) return false;
   const auto now = Clock::now();
   const auto deadline = now + kSliceBudget;
   {
      std::scoped_lock lock(g_mutex);
      g_surface = surfaceIndex;
   }
   dropRetired(deadline);
   if (g_job && g_job->surfaceIndex != surfaceIndex) g_job.reset();
   if (g_job) {
      Job& job = *g_job;
      if (!job.assembler.joinable()) {
         switch (walkSlice(job, deadline)) {
         case Walked::Some: break;
         case Walked::All: assembleApart(job); break;
         case Walked::Gone: g_job.reset(); break;
         }
         return false;
      }
      if (!job.assembled.load(std::memory_order_acquire)) return false;
      if (job.list && placeOf(job.game, job.playerIndex, job.surfaceIndex)) {
         install(job, true);
         // Only slow ones, as small surfaces run one every second.
         if (job.heldMs() + job.assembleMs >= 50) report(job, "automatic");
      }
      g_nextCycle = nextCycleAfter(job);
      g_job.reset();
      return false;
   }

   bool current;
   {
      std::scoped_lock lock(g_mutex);
      current = g_list.game == game && g_list.surfaceIndex == surfaceIndex;
   }
   if (current ? now < g_nextCycle : now < g_nextTry) return false;
   // One that cannot start is not tried again every tick.
   g_nextTry = now + kCycleInterval;
   return true;
}

void setModUiOpen(int playerIndex, bool open) {
   if (!localPlayer(currentGame(), playerIndex)) return;
   std::scoped_lock lock(g_mutex);
   g_modUiOpen = open;
}

void observeKey(const input::KeyEvent* key) {
   world::setKeyCursor(std::nullopt);
   if (!key || !key->down) return;
   const std::byte* game = currentGame();
   if (!game || !at<const std::byte*>(game, layout.gameLocalPlayer) || !world::drivesCursor()) return;
   // An open game window has these keys for itself (an inserter's hand size, in the mod).
   const std::byte* view = at<const std::byte*>(game, layout.gameView);
   if (!view || at<const void*>(view, layout.gameViewActiveWindow)) return;
   std::optional<Action> action = actionFor(*key);
   if (!action) return;
   std::scoped_lock lock(g_mutex);
   if (g_modUiOpen) return;
   if (std::optional<Position> target = act(*action)) world::setKeyCursor(world::CursorPosition{target->x, target->y});
}

std::optional<Entry> entryAt(int playerIndex, double x, double y) {
   if (!localPlayer(currentGame(), playerIndex)) return std::nullopt;
   const Position position{fixedPoint(x), fixedPoint(y)};
   std::scoped_lock lock(g_mutex);
   for (auto move = g_moves.rbegin(); move != g_moves.rend(); ++move) {
      if (move->target != position) continue;
      Entry entry = move->entry;
      // Older moves were said already or never will be.
      g_moves.erase(g_moves.begin(), move.base());
      return entry;
   }
   return std::nullopt;
}

std::optional<Category> category(int playerIndex) {
   if (!localPlayer(currentGame(), playerIndex)) return std::nullopt;
   std::scoped_lock lock(g_mutex);
   return std::exchange(g_category, std::nullopt);
}

} // namespace fa::scanner
