#include "scanner.h"

#include "bindings.h"
#include "chart.h"
#include "game.h"
#include "log.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace fa::scanner {

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

// Types whose subcategory says more than the prototype: what a machine makes, what a chest or pipe
// holds, which train a wagon is in, which network a roboport names. The Lua API reads all of it, so
// the mod's Lua gives these entities their subcategories (scripts/scanner/subcategories.lua).
constexpr std::string_view kDetailedTypes[] = {
   "artillery-wagon",
   "assembling-machine",
   "cargo-wagon",
   "container",
   "entity-ghost",
   "fluid-wagon",
   "furnace",
   "infinity-container",
   "infinity-pipe",
   "locomotive",
   "logistic-container",
   "mining-drill",
   "pipe",
   "pipe-to-ground",
   "roboport",
   "storage-tank",
   "tile-ghost",
   "unit-spawner",
};

// How an entity is listed.
enum class Listing : uint8_t { No, Alone, Tree, Resource };

struct Rule {
   Listing listing = Listing::No;
   Cat category = Other;
   bool detailed = false;
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
   if (type == "resource")
      return {Listing::Resource, Resources, false, at<bool>(prototype, layout.resourceInfinite),
              patchCellShift(prototype)};
   if (std::find(std::begin(kRocks), std::end(kRocks), name) != std::end(kRocks)) return {Listing::Alone, Resources};
   if (name.ends_with("-remnants")) return {Listing::Alone, Remnants};
   const bool detailed =
      std::find(std::begin(kDetailedTypes), std::end(kDetailedTypes), type) != std::end(kDetailedTypes);
   for (const TypeRule& rule : kTypes)
      if (rule.type == type) return {Listing::Alone, rule.category, detailed};
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
// they are left as they are, since their entities may be gone without having told them.
class Links {
public:
   Links() = default;
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

   // Links every entity, in order.
   Links(const std::byte* game, const std::vector<std::byte*>& entities) : targeters_(entities.size()), game_(game) {
      for (size_t i = 0; i < entities.size(); ++i) link(targeters_[i], entities[i]);
   }

   // Entity `index`, or null once the game dropped it.
   const std::byte* entity(size_t index) const { return targeters_[index].target; }

private:
   void release() {
      if (targeters_.empty()) return;
      if (game_ == currentGame()) {
         for (Targeter& targeter : targeters_) unlink(targeter);
      } else {
         // Still linked into a game that may not have let go of them: keep them in place for good.
         new std::vector<Targeter>(std::move(targeters_));
      }
      targeters_.clear();
   }

   std::vector<Targeter> targeters_;
   const std::byte* game_ = nullptr;
};

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
   bool detailed = false;           // the mod is to give it a subcategory of its own
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
   std::vector<Item> items;
   Links links;
   std::array<std::vector<Subcategory>, kCategoryCount> categories;
   std::vector<uint32_t> detailed;  // the items handed to the mod for subcategories, in order
   std::vector<uint8_t> tileClasses; // TileClass by tile ID
};

// Groups the items into categories and subcategories, every item into All as well, nearest first:
// the entries of each subcategory, then the subcategories by their nearest.
void group(List& list) {
   std::array<std::unordered_map<std::string_view, size_t>, kCategoryCount> index;
   for (auto& subcategories : list.categories) subcategories.clear();
   auto add = [&](Cat category, const std::string& key, uint32_t item) {
      auto [found, added] = index[category].try_emplace(key, list.categories[category].size());
      if (added) list.categories[category].push_back({key, {}});
      list.categories[category][found->second].items.push_back(item);
   };
   for (uint32_t i = 0; i < list.items.size(); ++i) {
      const Item& item = list.items[i];
      add(All, item.key, i);
      if (item.category != All) add(item.category, item.key, i);
   }

   for (auto& subcategories : list.categories) {
      for (Subcategory& subcategory : subcategories) {
         std::vector<std::pair<double, uint32_t>> keyed;
         keyed.reserve(subcategory.items.size());
         for (uint32_t item : subcategory.items)
            keyed.emplace_back(distanceSquared(list.items[item].position, list.origin), item);
         std::sort(keyed.begin(), keyed.end());
         for (size_t i = 0; i < keyed.size(); ++i) subcategory.items[i] = keyed[i].second;
      }
      std::stable_sort(subcategories.begin(), subcategories.end(), [&](const Subcategory& a, const Subcategory& b) {
         return distanceSquared(list.items[a.items.front()].position, list.origin) <
                distanceSquared(list.items[b.items.front()].position, list.origin);
      });
   }
}

// An entity met in the walk.
struct Found {
   std::byte* entity;
   const std::byte* prototype;
   Position position;
};

// Trees and resources, counted by cell rather than kept one by one. Touching cells of one group
// (all trees, or one resource prototype) make one forest or patch. Every cell lies in one chunk, as
// the cells' sides divide 32.
class Cells {
public:
   explicit Cells(Position origin) : origin_(origin) {}

   // Starts the next chunk: the walk adds each chunk's entities together, all standing in it.
   void beginChunk() {
      ++generation_;
      groupsHere_ = 0;
   }

   // Puts `found` in its cell of `group`, cells being 2^shift tiles a side. One listed alone only
   // joins the cells around it: it is neither counted nor the forest's or patch's place.
   void add(const std::byte* group, int shift, const Found& found, bool counted) {
      const int32_t x = tileOf(found.position.x), y = tileOf(found.position.y);
      // The chunk's own table of the group's cells, by place in the chunk.
      Local* local = nullptr;
      for (size_t i = 0; i < groupsHere_; ++i)
         if (locals_[i].group == group) local = &locals_[i];
      if (!local) {
         if (groupsHere_ == locals_.size()) locals_.emplace_back();
         local = &locals_[groupsHere_++];
         local->group = group;
      }
      const size_t slot = static_cast<size_t>(((x & 31) >> shift) + ((y & 31) >> shift) * (32 >> shift));
      if (local->stamps[slot] != generation_) {
         local->stamps[slot] = generation_;
         local->cells[slot] = cellAt({group, x >> shift, y >> shift});
      }
      Cell& cell = cells_[local->cells[slot]];
      if (!counted) return;
      ++cell.count;
      cell.box.add(x, y);
      const double distance = distanceSquared(found.position, origin_);
      if (distance < cell.nearest) {
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

   std::vector<Group> groups() const {
      Sets sets;
      for (size_t i = 0; i < cells_.size(); ++i) sets.add();
      // Half the 8 neighbours: the other half joins from the far side.
      constexpr std::array<std::pair<int32_t, int32_t>, 4> kNeighbours{{{1, 0}, {1, 1}, {0, 1}, {-1, 1}}};
      for (uint32_t i = 0; i < cells_.size(); ++i) {
         const Key& key = cells_[i].key;
         for (auto [dx, dy] : kNeighbours)
            if (auto other = index_.find({key.group, key.x + dx, key.y + dy}); other != index_.end())
               sets.join(i, other->second);
      }

      std::vector<uint32_t> groupOf(cells_.size(), UINT32_MAX);
      std::vector<Group> groups;
      for (uint32_t i = 0; i < cells_.size(); ++i) {
         const uint32_t root = sets.find(i);
         if (groupOf[root] == UINT32_MAX) {
            groupOf[root] = static_cast<uint32_t>(groups.size());
            groups.push_back({cells_[i].key.group});
         }
         const Cell& cell = cells_[i];
         Group& group = groups[groupOf[root]];
         group.count += cell.count;
         if (cell.count == 0) continue;
         group.box.add(cell.box);
         if (cell.nearest < group.nearest) {
            group.nearest = cell.nearest;
            group.position = cell.position;
            group.prototype = cell.prototype;
         }
      }
      std::erase_if(groups, [](const Group& group) { return group.count == 0; });
      return groups;
   }

   size_t size() const { return cells_.size(); }

private:
   struct Key {
      const std::byte* group;
      int32_t x;
      int32_t y;
      bool operator==(const Key&) const = default;
   };
   struct KeyHash {
      size_t operator()(const Key& key) const {
         return std::hash<const void*>{}(key.group) ^
                std::hash<uint64_t>{}(packCell(key.x, key.y) * 0x9e3779b97f4a7c15);
      }
   };
   struct Cell {
      Key key;
      uint32_t count = 0;
      double nearest = INFINITY;
      Position position{};
      const std::byte* prototype = nullptr;
      TileBox box;
   };

   // One group's cells in the current chunk, by place: valid where the stamp is the chunk's
   // generation, so a table taken over from an earlier chunk needs no clearing.
   struct Local {
      const std::byte* group = nullptr;
      std::array<uint32_t, 1024> stamps{};
      std::array<uint32_t, 1024> cells{};
   };

   uint32_t cellAt(const Key& key) {
      auto [index, added] = index_.try_emplace(key, static_cast<uint32_t>(cells_.size()));
      if (added) cells_.push_back({key});
      return index->second;
   }

   Position origin_;
   std::vector<Cell> cells_;
   std::unordered_map<Key, uint32_t, KeyHash> index_;
   std::deque<Local> locals_;
   size_t groupsHere_ = 0;
   uint32_t generation_ = 0;
};

// Builds a list's items from what one refresh walked, and the entities to link, in item order.
class Builder {
public:
   Builder(List& list, std::vector<std::byte*>& entities) : list_(list), entities_(entities) {}

   void alone(const Found& found, const Rule& rule) {
      if (rule.detailed) list_.detailed.push_back(static_cast<uint32_t>(list_.items.size()));
      add({Kind::Entity, rule.category, rule.detailed, found.prototype, found.position,
           std::string(prototypeName(found.prototype))},
          found);
   }

   // A tree, or a well of an infinite resource, near enough the origin to be listed by itself.
   void near(const Found& found, std::string key) {
      add({Kind::Entity, Resources, false, found.prototype, found.position, std::move(key)}, found);
   }

   // A resource whose prototype makes every resource a patch of its own.
   void patchOfOne(const Found& found) {
      Item item{Kind::Patch,     Resources,      false,
                found.prototype, found.position, std::string(prototypeName(found.prototype))};
      item.count = 1;
      item.box.add(tileOf(found.position.x), tileOf(found.position.y));
      list_.items.push_back(std::move(item));
   }

   // The forests (trees grouped under null) and patches of `cells`.
   void groups(const Cells& cells) {
      for (const Cells::Group& group : cells.groups()) {
         const bool forest = !group.group;
         Item item{forest ? Kind::Forest : Kind::Patch,
                   Resources,
                   false,
                   forest ? group.prototype : group.group,
                   group.position,
                   forest ? "tree" : std::string(prototypeName(group.group))};
         item.count = group.count;
         item.box = group.box;
         list_.items.push_back(std::move(item));
      }
   }

private:
   void add(Item item, const Found& found) {
      item.first = static_cast<uint32_t>(entities_.size());
      item.count = 1;
      entities_.push_back(found.entity);
      list_.items.push_back(std::move(item));
   }

   List& list_;
   std::vector<std::byte*>& entities_;
};

// Bodies of water or ice: connected tiles of one class, 8 ways, chunk by chunk. Each chunk labels its
// own components; components touching across chunk borders are then joined.
class TileBodies {
public:
   explicit TileBodies(TileClass tileClass) : class_(tileClass) {}

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

   void setOrigin(Position origin) { origin_ = origin; }

   // Each body: its box, and its tile nearest the origin.
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
      std::vector<Body> result;
      result.reserve(roots.size());
      for (const auto& [root, stats] : roots) result.push_back({stats.box, stats.nearestX, stats.nearestY});
      return result;
   }

private:
   static constexpr uint16_t kUnlabelled = 0xffff;

   struct Stats {
      TileBox box;
      double nearest = INFINITY;
      int32_t nearestX = 0;
      int32_t nearestY = 0;
      void add(int32_t x, int32_t y, Position origin) {
         box.add(x, y);
         const double distance = distanceSquared(tileCentre(x, y), origin);
         if (distance < nearest) {
            nearest = distance;
            nearestX = x;
            nearestY = y;
         }
      }
      void add(const Stats& other) {
         box.add(other.box);
         if (other.nearest < nearest) {
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

// The world the list was made in, while it is still there.
struct World {
   const std::byte* surface = nullptr;
   explicit operator bool() const { return surface; }
};

World worldOf(const List& list) {
   const std::byte* game = currentGame();
   if (!game || game != list.game) return {};
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
   case Kind::Extra: entry.extra = item.first + 1; break;
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

} // namespace

std::optional<std::vector<Detail>> refresh(const Refresh& request) {
   Stopwatch stopwatch;
   const std::byte* game = currentGame();
   const std::byte* player = localPlayer(game, request.playerIndex);
   if (!player) return std::nullopt;
   const std::byte* map = at<const std::byte*>(player, layout.playerMap);
   const std::byte* surface = map ? surfaceAt(map, request.surfaceIndex) : nullptr;
   if (!surface) {
      log::error("Scanner: no surface {} to list", request.surfaceIndex);
      return std::nullopt;
   }
   const uint32_t surfaceIndex = at<uint32_t>(surface, layout.surfaceIndex);

   List list;
   list.game = game;
   list.surfaceIndex = request.surfaceIndex;
   list.force = forceOf(player);
   list.origin = {fixedPoint(request.x), fixedPoint(request.y)};
   list.tileClasses = tileClasses(request);
   const double radiusSquared = request.radius * request.radius;
   auto wanted = [&](Position position) {
      return distanceSquared(position, list.origin) < radiusSquared &&
             (!request.direction || directionBiased(position, list.origin) == *request.direction);
   };

   std::vector<std::byte*> entities;
   Builder builder(list, entities);
   std::unordered_map<const std::byte*, Rule> rules;
   const std::byte* lastPrototype = nullptr;
   const Rule* lastRule = nullptr;
   Cells cells(list.origin);
   size_t trees = 0, resources = 0;
   const double treeZoomSquared = kForestZoomDistance * kForestZoomDistance;
   const double wellZoomSquared = kInfiniteResourceZoomDistance * kInfiniteResourceZoomDistance;
   TileBodies water(WaterTile);
   TileBodies ice(IceTile);
   water.setOrigin(list.origin);
   ice.setOrigin(list.origin);

   const auto& chunks = at<MsvcVector<const std::byte*>>(surface, layout.surfaceChunks);
   size_t chunksWalked = 0;
   for (const std::byte* const* chunkSlot = chunks.first; chunkSlot < chunks.last; ++chunkSlot) {
      const std::byte* chunk = *chunkSlot;
      if (!chunk) continue;
      const Position chunkPosition = at<Position>(chunk, layout.chunkPosition);
      // The chunk's nearest point to the origin, in tiles.
      const double left = chunkPosition.x * 32.0, top = chunkPosition.y * 32.0;
      const double nearX = std::clamp(request.x, left, left + 32), nearY = std::clamp(request.y, top, top + 32);
      if ((nearX - request.x) * (nearX - request.x) + (nearY - request.y) * (nearY - request.y) > radiusSquared)
         continue;
      const Position centre{fixedPoint(left + 16), fixedPoint(top + 16)};
      if (!reinterpret_cast<ChartedFunction>(layout.forceIsChunkCharted)(list.force, surfaceIndex, &centre)) continue;

      const Position first{chunkPosition.x * 16, chunkPosition.y * 16};
      const Position last{first.x + 15, first.y + 15};
      cells.beginChunk();
      forEachEntity(surface, first, last, [&](const std::byte* entity) {
         // The iterator also gives entities standing just past the chunk's edge, which their own
         // chunk gives again: each chunk keeps those standing in it.
         const Position position = at<Position>(entity, layout.entityPosition);
         if ((tileOf(position.x) >> 5) != chunkPosition.x || (tileOf(position.y) >> 5) != chunkPosition.y) return;
         if (at<uint16_t>(entity, layout.entityUsageBits) & kNotListedBits) return;
         const std::byte* prototype = at<const std::byte*>(entity, layout.entityPrototypeOf);
         if (prototype != lastPrototype) {
            auto [found, added] = rules.try_emplace(prototype);
            if (added) found->second = ruleFor(prototype, prototypeType(prototype), prototypeName(prototype));
            lastPrototype = prototype;
            lastRule = &found->second;
         }
         const Rule& rule = *lastRule;
         if (rule.listing == Listing::No) return;
         if (!wanted(position)) return;
         const Found found{const_cast<std::byte*>(entity), prototype, position};
         switch (rule.listing) {
         case Listing::Alone: builder.alone(found, rule); break;
         case Listing::Tree: {
            ++trees;
            // Trees near the origin are listed alone, but still join the forest around them.
            const bool near = distanceSquared(position, list.origin) < treeZoomSquared;
            if (near) builder.near(found, "tree");
            cells.add(nullptr, kForestCellShift, found, !near);
            break;
         }
         case Listing::Resource: {
            ++resources;
            if (rule.cellShift < 0) {
               builder.patchOfOne(found);
               break;
            }
            // So are the wells of an infinite resource.
            const bool near = rule.infinite && distanceSquared(position, list.origin) < wellZoomSquared;
            if (near) builder.near(found, std::string(prototypeName(prototype)));
            cells.add(prototype, rule.cellShift, found, !near);
            break;
         }
         case Listing::No: break;
         }
      });
      water.addChunk(chunk, chunkPosition.x, chunkPosition.y, list.tileClasses);
      ice.addChunk(chunk, chunkPosition.x, chunkPosition.y, list.tileClasses);
      ++chunksWalked;
   }
   const double walkMs = stopwatch.lap();

   builder.groups(cells);
   for (auto [bodies, kind, category, key] :
        {std::tuple{&water, Kind::Water, Resources, "water"}, std::tuple{&ice, Kind::Ice, Terrain, "iceberg"}}) {
      for (const TileBodies::Body& body : bodies->bodies()) {
         const Position nearest = tileCentre(body.nearestX, body.nearestY);
         if (!wanted(nearest)) continue;
         Item item{kind, category, false, nullptr, nearest, key};
         item.box = body.box;
         list.items.push_back(std::move(item));
      }
   }
   for (size_t i = 0; i < request.extras.size(); ++i) {
      const Extra& extra = request.extras[i];
      const std::optional<Cat> category = categoryByKey(extra.category);
      if (!category) {
         log::error("Scanner: no category {} for an entry of the mod's", extra.category);
         continue;
      }
      const Position position{fixedPoint(extra.x), fixedPoint(extra.y)};
      if (!wanted(position) ||
          !reinterpret_cast<ChartedFunction>(layout.forceIsChunkCharted)(list.force, surfaceIndex, &position))
         continue;
      Item item{Kind::Extra, *category, false, nullptr, position, extra.key};
      item.first = static_cast<uint32_t>(i);
      list.items.push_back(std::move(item));
   }
   const double clusterMs = stopwatch.lap();
   list.links = Links(game, entities);
   const double linkMs = stopwatch.lap();
   group(list);
   const double groupMs = stopwatch.lap();

   std::vector<Detail> details;
   details.reserve(list.detailed.size());
   for (uint32_t item : list.detailed)
      details.push_back({list.items[item].key, tiles(list.items[item].position.x), tiles(list.items[item].position.y)});

   std::scoped_lock lock(g_mutex);
   // The category stays; the rest starts over, as the list beneath it is new.
   Cursor cursor{g_cursor.category.value_or(All), {}, {}};
   g_list = std::move(list);
   g_cursor = cursor;
   log::info("Scanner: {} entries ({} linked, {} trees, {} resources in {} cells) on surface {}, {} for the mod "
             "to detail",
             g_list.items.size(), entities.size(), trees, resources, cells.size(), request.surfaceIndex,
             details.size());
   log::info("Scanner: {} chunks walked in {:.1f} ms, clustered in {:.1f}, linked in {:.1f}, grouped in {:.1f}, "
             "the rest {:.1f}",
             chunksWalked, walkMs, clusterMs, linkMs, groupMs, stopwatch.lap());
   return details;
}

void setSubcategories(int playerIndex, const std::vector<std::optional<std::string>>& keys) {
   if (!localPlayer(currentGame(), playerIndex)) return;
   std::scoped_lock lock(g_mutex);
   if (keys.size() != g_list.detailed.size()) {
      log::error("Scanner: {} subcategories for {} entries", keys.size(), g_list.detailed.size());
      return;
   }
   for (size_t i = 0; i < keys.size(); ++i)
      if (keys[i]) g_list.items[g_list.detailed[i]].key = *keys[i];
   g_list.detailed.clear();
   group(g_list);
   g_cursor = {g_cursor.category.value_or(All), {}, {}};
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
