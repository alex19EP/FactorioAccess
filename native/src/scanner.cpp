#include "scanner.h"

#include "bindings.h"
#include "game.h"
#include "log.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <deque>
#include <mutex>
#include <string_view>
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
   "all",        "build_spots",         "pins",       "tags",     "resources", "enemies",
   "remnants",   "production",          "logistics_and_power",    "containers", "military",
   "vehicles",   "spidertrons",         "trains",     "ghosts",   "players",   "corpses",
   "other",      "terrain",
};

// The category of each entity type the scanner lists. Types left out (beams, explosions,
// particles ...) are not listed.
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
   "big-rock",   "big-sand-rock", "huge-rock",       "medium-rock",
   "medium-sand-rock", "small-rock", "small-sand-rock", "tiny-rock",
};
static_assert(!kCategoryKeys.back().empty(), "a category has no key");

struct Rule {
   bool listed = false;
   Cat category = Other;
};

Rule ruleFor(std::string_view type, std::string_view name) {
   if (std::find(std::begin(kRocks), std::end(kRocks), name) != std::end(kRocks)) return {true, Resources};
   if (name.ends_with("-remnants")) return {true, Remnants};
   for (const TypeRule& rule : kTypes)
      if (rule.type == type) return {true, rule.category};
   return {};
}

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

std::string_view prototypeName(const std::byte* prototype) {
   return view(at<MsvcString>(prototype, layout.prototypeName));
}

std::string_view prototypeType(const std::byte* prototype) {
   return virtualAt<const char* (*)(const void*)>(prototype, layout.prototypeGetType)(prototype);
}

uint64_t unitNumber(const std::byte* entity) {
   return reinterpret_cast<uint64_t (*)(const void*)>(layout.entityUnitNumber)(entity);
}

struct TreeNode {
   TreeNode* left;
   TreeNode* parent;
   TreeNode* right;
   char colour;
   char isNil;
};
constexpr uint32_t kTreeKey = 0x20;
constexpr uint32_t kTreeValue = 0x28;

// Map::unitNumberToEntity[number], or null.
const std::byte* entityByUnitNumber(const std::byte* map, uint64_t number) {
   auto* head = at<TreeNode*>(map, layout.mapUnitNumbers);
   for (TreeNode* node = head->parent; !node->isNil;) {
      uint64_t key = at<uint64_t>(node, kTreeKey);
      if (key == number) return at<const std::byte*>(node, kTreeValue);
      node = key < number ? node->right : node->left;
   }
   return nullptr;
}

using IteratorFunction = void (*)(std::byte* iterator);

// Calls `visit` with each entity on `surface` whose position lies in the advanced tiles (two tiles
// a side) from `first` to `last`, each once.
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

using ChartedFunction = bool (*)(const void* force, uint32_t surfaceIndex, const Position* position);

// FaUtils.get_direction_biased: the 8-way defines.direction (16-way numbering) from `origin` to
// `target`, by whole tiles, keeping to a cardinal unless the diagonal is clear.
int directionBiased(Position target, Position origin) {
   int dx = (target.x >> 8) - (origin.x >> 8);
   int dy = (target.y >> 8) - (origin.y >> 8);
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

// An entry: one entity.
struct Item {
   const std::byte* entity;  // compared only, never read, until found alive again
   const std::byte* prototype;
   uint64_t unitNumber;      // 0 for entities without one
   Position position;
};

struct Subcategory {
   std::string key;
   std::vector<uint32_t> items; // into List::items
};

struct List {
   const std::byte* game = nullptr;
   uint32_t surfaceIndex = 0;
   Position origin{};
   std::vector<Item> items;
   std::array<std::vector<Subcategory>, kCategoryCount> categories;
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
   {"fa-pageup", Action::SubcategoryBack},   {"fa-pagedown", Action::SubcategoryNext},
   {"fa-s-pageup", Action::EntryBack},       {"fa-s-pagedown", Action::EntryNext},
   {"fa-c-pageup", Action::CategoryBack},    {"fa-c-pagedown", Action::CategoryNext},
   {"fa-home", Action::Repeat},
};

std::mutex g_mutex;
List g_list;
Cursor g_cursor;
std::deque<Move> g_moves;
std::optional<Category> g_category;
bool g_modUiOpen = false;

// The live entity an item stands for, or null when it is gone. Reads the world, so only while it
// stands still.
const std::byte* liveEntity(const std::byte* map, const std::byte* surface, const Item& item) {
   // Moving entities (units, vehicles, robots) have unit numbers; the map keeps an index of them.
   if (item.unitNumber) {
      if (const std::byte* entity = entityByUnitNumber(map, item.unitNumber))
         return entity == item.entity && at<const std::byte*>(entity, layout.entitySurface) == surface ? entity
                                                                                                       : nullptr;
   }
   const Position tile{item.position.x >> 9, item.position.y >> 9};
   const std::byte* found = nullptr;
   forEachEntity(surface, tile, tile, [&](const std::byte* entity) {
      if (!found && entity == item.entity && at<const std::byte*>(entity, layout.entityPrototypeOf) == item.prototype)
         found = entity;
   });
   return found;
}

// The world the list was made in, while it is still there.
struct World {
   const std::byte* map = nullptr;
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
   return {map, surfaceAt(map, list.surfaceIndex)};
}

// The entity of entry `index` of `items` when it is still there, with its position brought up to
// date.
const std::byte* validate(const World& world, uint32_t index) {
   Item& item = g_list.items[index];
   const std::byte* entity = liveEntity(world.map, world.surface, item);
   if (entity) item.position = at<Position>(entity, layout.entityPosition);
   return entity;
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
      move.target = item.position;
      move.entry.index = place->first + 1;
      move.entry.count = place->second;
      move.entry.prototype = std::string(prototypeName(item.prototype));
      move.entry.x = tiles(item.position.x);
      move.entry.y = tiles(item.position.y);
      move.entry.originX = tiles(g_list.origin.x);
      move.entry.originY = tiles(g_list.origin.y);
   }
   std::optional<Position> target = move.target;
   remember(std::move(move));
   return target;
}

} // namespace

bool refresh(const Refresh& request) {
   const std::byte* game = currentGame();
   const std::byte* player = localPlayer(game, request.playerIndex);
   if (!player) return false;
   const std::byte* map = at<const std::byte*>(player, layout.playerMap);
   const std::byte* surface = map ? surfaceAt(map, request.surfaceIndex) : nullptr;
   if (!surface) {
      log::error("Scanner: no surface {} to list", request.surfaceIndex);
      return false;
   }
   const std::byte* force = at<const std::byte* const*>(map, layout.mapForces)[at<uint8_t>(player, layout.playerForce)];
   const uint32_t surfaceIndex = at<uint32_t>(surface, layout.surfaceIndex);

   List list;
   list.game = game;
   list.surfaceIndex = request.surfaceIndex;
   list.origin = {fixedPoint(request.x), fixedPoint(request.y)};
   const double radiusSquared = request.radius * request.radius;

   std::unordered_map<const std::byte*, Rule> rules;
   std::array<std::unordered_map<std::string, size_t>, kCategoryCount> subcategoryIndex;
   auto add = [&](Cat category, const std::string& key, uint32_t item) {
      auto [found, added] = subcategoryIndex[category].try_emplace(key, list.categories[category].size());
      if (added) list.categories[category].push_back({key, {}});
      list.categories[category][found->second].items.push_back(item);
   };

   const auto& chunks = at<MsvcVector<const std::byte*>>(surface, layout.surfaceChunks);
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
      if (!reinterpret_cast<ChartedFunction>(layout.forceIsChunkCharted)(force, surfaceIndex, &centre)) continue;

      const Position first{chunkPosition.x * 16, chunkPosition.y * 16};
      const Position last{first.x + 15, first.y + 15};
      forEachEntity(surface, first, last, [&](const std::byte* entity) {
         const std::byte* prototype = at<const std::byte*>(entity, layout.entityPrototypeOf);
         auto [rule, added] = rules.try_emplace(prototype);
         if (added) rule->second = ruleFor(prototypeType(prototype), prototypeName(prototype));
         if (!rule->second.listed) return;
         const Position position = at<Position>(entity, layout.entityPosition);
         if (distanceSquared(position, list.origin) >= radiusSquared) return;
         if (request.direction && directionBiased(position, list.origin) != *request.direction) return;
         const auto item = static_cast<uint32_t>(list.items.size());
         list.items.push_back({entity, prototype, unitNumber(entity), position});
         const std::string key(prototypeName(prototype));
         add(All, key, item);
         if (rule->second.category != All) add(rule->second.category, key, item);
      });
   }

   // Nearest first: the entries of each subcategory, then the subcategories by their nearest.
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

   std::scoped_lock lock(g_mutex);
   // The category stays; the rest starts over, as the list beneath it is new.
   Cursor cursor{g_cursor.category.value_or(All), {}, {}};
   g_list = std::move(list);
   g_cursor = cursor;
   log::info("Scanner: {} entries on surface {}", g_list.items.size(), request.surfaceIndex);
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
