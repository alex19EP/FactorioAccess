#include "railplanner.h"

#include "game.h"
#include "reader.h"
#include "speech.h"
#include "text.h"
#include "vocab.h"
#include "world.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fa::railplanner {

namespace {

using game::layout;

// Directions run 0 to 15; 16 is none.
constexpr uint8_t kDirections = 16;

template <class T>
T at(const void* object, uint32_t offset) {
   T value;
   std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
   return value;
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

// A RailLocation: where a rail end is, which way it leads and on which layer.
struct Location {
   int32_t x = 0;
   int32_t y = 0;
   uint8_t direction = 0;
   uint8_t layer = 0;
   bool operator==(const Location&) const = default;
};

Location location(const void* railLocation) {
   const auto* self = static_cast<const std::byte*>(railLocation);
   return {at<int32_t>(self, layout.railLocationPosition), at<int32_t>(self, layout.railLocationPosition + 4),
           at<uint8_t>(self, layout.railLocationDirection), at<uint8_t>(self, layout.railLocationLayer)};
}

// What the planner searches: from its start to the cursor's goal, building ghosts or not. The
// search starts over when any of it changes.
struct Search {
   Location start;
   Location goal;
   bool ghost = false;
   bool operator==(const Search&) const = default;
};

// This client's GameView, which holds the planner; null outside a game.
const std::byte* gameView() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   if (!context) return nullptr;
   auto* game = at<const std::byte*>(context, layout.globalGame);
   return game ? at<const std::byte*>(game, layout.gameView) : nullptr;
}

// One item count the game draws beside the cursor.
struct Count {
   uint16_t item;
   uint8_t quality;
   uint32_t count;
};

// A rail of the best route as RailBuildPlan holds it.
struct Placed {
   uint16_t prototype;
   int32_t x;
   int32_t y;
   uint8_t direction;
};

// What one drawRailPlanBuilder call of this client drew: the route's rails, its end arrow and its
// counts.
struct Drawn {
   bool planned = false;
   Location planStart;
   std::vector<Placed> rails;
   bool arrow = false;
   Location end;
   bool tooFar = false;
   std::vector<Count> counts;
};

// Set while drawRailPlanBuilder draws for the player the mod drives. Only the drawing thread
// touches these.
thread_local Drawn* t_drawn = nullptr;

// What was said, kept between frames on the drawing thread.
struct State {
   std::chrono::steady_clock::time_point lastFrame;
   bool active = false;
   Location start;
   std::optional<Search> spoken;
   // The last route said: the next goal, often a tile away on the two-tile rail grid, tends to give
   // the same route.
   std::string summary;
   uint32_t steps = 0;
   // The end of the route drawn last frame: a start moved there was built.
   std::optional<Location> lastEnd;
   // The layer the planner builds on.
   uint8_t layer = 0;
   // The search drawn last and since when: a goal on a rail end snaps to it once the game selects
   // the rail, a few frames after the cursor moves there.
   std::optional<Search> seen;
   std::chrono::steady_clock::time_point seenSince;
};
State g_state;

// Set by sayRoute: the next route is said even when it reads as the one before.
std::atomic<bool> g_sayRoute{false};

// The route drawn last, for the route key: written on the drawing thread, read from Lua.
std::mutex g_routeMutex;
std::optional<Route> g_route;
// When it was drawn: the planner draws each frame while it plans, and draws nothing once the
// rails leave the hand.
std::chrono::steady_clock::time_point g_routeTime;

void publish(std::optional<Route> route) {
   const std::lock_guard lock(g_routeMutex);
   g_route = std::move(route);
   g_routeTime = std::chrono::steady_clock::now();
}

// The prototype an ID indexes in a PrototypeList<T>::indexToPrototype vector, or null.
const std::byte* prototypeAt(uintptr_t list, size_t index) {
   const auto& prototypes = *reinterpret_cast<const MsvcVector<const std::byte* const>*>(list);
   return index < static_cast<size_t>(prototypes.last - prototypes.first) ? prototypes.first[index] : nullptr;
}

std::string_view typeName(const std::byte* prototype) {
   using GetType = const char* (*)(const void*);
   const auto* table = *reinterpret_cast<GetType const* const*>(prototype);
   return table[layout.prototypeGetType](prototype);
}

// The route rail by rail: each leads on from the end the one before it reached, out of its other
// end, as RailPrototype::connectionPoints place the two ends for its direction.
Route walk(const Location& start, const std::vector<Placed>& rails) {
   Route route{{start.x, start.y, start.direction, start.layer != 0}, {}};
   int32_t x = start.x;
   int32_t y = start.y;
   for (const Placed& rail : rails) {
      const std::byte* prototype = prototypeAt(layout.entityPrototypes, rail.prototype);
      if (!prototype) break;
      Location ends[2];
      for (size_t end = 0; end < 2; ++end) {
         const std::byte* point =
            prototype + layout.railConnectionPoints + (size_t{rail.direction} * 2 + end) * layout.connectionPointSize;
         ends[end] = location(point);
         ends[end].x += rail.x;
         ends[end].y += rail.y;
      }
      auto distance = [&](const Location& end) { return std::max(std::abs(end.x - x), std::abs(end.y - y)); };
      const Location& out = distance(ends[0]) >= distance(ends[1]) ? ends[0] : ends[1];
      route.pieces.push_back({std::string(typeName(prototype)), {out.x, out.y, out.direction, out.layer != 0}});
      x = out.x;
      y = out.y;
   }
   return route;
}

using LocalisedStr = const MsvcString* (*)(const void* localisedString, const void* localeProvider);

std::string localisedName(const std::byte* prototype) {
   const MsvcString* name =
      reinterpret_cast<LocalisedStr>(layout.localisedStringStr)(prototype + layout.prototypeLocalisedName, nullptr);
   return text::speakable(view(*name));
}

// "Rail x 12", as the mod says an item and its count.
std::string countText(const Count& count) {
   const std::byte* item = prototypeAt(layout.itemPrototypes, count.item);
   if (!item) return {};
   const std::byte* quality = prototypeAt(layout.qualityPrototypes, count.quality);
   const bool hasQuality = quality && view(at<MsvcString>(quality, layout.prototypeName)) != "normal";
   return vocab::kItemQuantityQuality(localisedName(item), hasQuality ? 1 : 0,
                                      hasQuality ? localisedName(quality) : std::string(), count.count > 1 ? 1 : 0,
                                      count.count);
}

// The node of a std::map (MSVC): left, parent, right, colour, end marker, then the stored pair.
struct TreeNode {
   TreeNode* left;
   TreeNode* parent;
   TreeNode* right;
   char colour;
   char isNil;
};

// The entries of a std::map<IDWithQuality<ID<ItemPrototype>>, unsigned>.
std::vector<Count> readCounts(const void* map) {
   std::vector<Count> counts;
   auto* head = at<const TreeNode*>(map, 0);
   std::vector<const TreeNode*> pending{head->parent};
   while (!pending.empty()) {
      const TreeNode* node = pending.back();
      pending.pop_back();
      if (node->isNil) continue;
      const auto* self = reinterpret_cast<const std::byte*>(node);
      const std::byte* id = self + layout.countNodeId;
      counts.push_back({at<uint16_t>(id, layout.idWithQualityBase), at<uint8_t>(id, layout.idWithQualityQuality),
                        at<uint32_t>(self, layout.countNodeCount)});
      pending.push_back(node->left);
      pending.push_back(node->right);
   }
   std::stable_sort(counts.begin(), counts.end(), [](const Count& a, const Count& b) { return a.count > b.count; });
   return counts;
}

// The route as the game shows it once its search has stopped. `snapped`: the goal is the end of the
// rail under the cursor, which the route joins when it reaches it.
std::string summary(const Drawn& drawn, const Search& search, bool snapped) {
   if (!drawn.arrow) return vocab::kRailPlanNoRoute;
   std::string said;
   for (const Count& count : drawn.counts) {
      if (count.count == 0) continue;
      if (!said.empty()) said += ", ";
      said += countText(count);
   }
   if (!said.empty()) said += ", ";
   const std::string direction = vocab::kDirection(drawn.end.direction);
   said += drawn.end.layer != 0 ? vocab::kRailPlanEndsElevated(direction) : vocab::kRailPlanEnds(direction);
   // A route the game could not take all the way stops short of the cursor. A rail end lies on the
   // two-tile rail grid, so one that reaches the cursor is within two tiles of it.
   const Location& goal = search.goal;
   const int32_t shortBy =
      std::max(std::abs(drawn.end.x - goal.x), std::abs(drawn.end.y - goal.y)) / game::kMapPositionScale;
   // A rail end under the cursor is the goal itself: missing it by any distance leaves a gap.
   const bool reached = drawn.end.x == goal.x && drawn.end.y == goal.y;
   if (snapped && reached)
      said += ", " + std::string(vocab::kRailPlanJoins);
   else if (snapped ? !reached : shortBy > 2)
      said += ", " + vocab::kRailPlanShort(std::max(shortBy, 1));
   if (drawn.tooFar) said += ", " + std::string(vocab::kOutOfReach);
   if (search.ghost) said += ", " + std::string(vocab::kRailPlanGhosts);
   return said;
}

// After each frame's planning: what changed since the frame before.
void update(const Drawn& drawn) {
   const std::byte* gameViewObject = gameView();
   if (!gameViewObject) return;
   State& state = g_state;
   const auto now = std::chrono::steady_clock::now();
   // A gap in the frames is a time without rails in hand: what was said then no longer stands.
   if (now - state.lastFrame > std::chrono::milliseconds(500)) state = {};
   state.lastFrame = now;

   if (!at<bool>(gameViewObject, layout.gameViewRailPlannerActive)) {
      if (state.active) {
         speech::say(std::string(vocab::kRailPlanEnded));
         publish(std::nullopt);
      }
      state.active = false;
      state.spoken.reset();
      state.lastEnd.reset();
      return;
   }
   const std::byte* finder = at<const std::byte*>(gameViewObject, layout.gameViewRailPlanFinder);
   if (!finder) return;
   const Search search{location(gameViewObject + layout.gameViewRailPlanStart),
                       location(finder + layout.railPlanFinderGoal), at<bool>(finder, layout.railPlanFinderGhostMode)};
   const auto steps = at<uint32_t>(finder, layout.railPlanFinderStepCount);
   const auto layer = at<uint8_t>(gameViewObject, layout.gameViewRailLayer);
   const std::optional<Location> lastEnd = state.lastEnd;
   state.lastEnd = drawn.arrow ? std::optional(drawn.end) : std::nullopt;
   publish(drawn.planned ? std::optional(walk(drawn.planStart, drawn.rails)) : std::nullopt);

   if (!state.active || search.start != state.start) {
      const bool built = state.active && lastEnd && *lastEnd == search.start;
      const std::string direction = vocab::kDirection(search.start.direction);
      if (search.start.layer != 0)
         speech::say(built ? vocab::kRailPlanBuiltElevated(direction) : vocab::kRailPlanStartElevated(direction));
      else
         speech::say(built ? vocab::kRailPlanBuilt(direction) : vocab::kRailPlanStart(direction));
      state.active = true;
      state.start = search.start;
      // The cursor is still where planning started; the route to it is said once it moves.
      state.spoken = search;
      state.summary.clear();
      state.steps = steps;
      state.layer = layer;
      return;
   }
   // The game says only that the layer switched.
   if (layer != state.layer) {
      state.layer = layer;
      speech::say(std::string(layer != 0 ? vocab::kRailPlanLayerElevated : vocab::kRailPlanLayerGround));
   }
   if (state.seen != search) {
      state.seen = search;
      state.seenSince = now;
   }
   // The search runs a few hundred steps a frame and stops at a solution, a dead end or its limit.
   const bool stopped = steps == state.steps;
   state.steps = steps;
   const bool settled = now - state.seenSince >= std::chrono::milliseconds(100);
   if (!stopped || !settled || state.spoken == search) return;
   state.spoken = search;
   // Without a build direction the goal takes the direction of the rail end under the cursor, when
   // there is one: the direction the cursor has otherwise.
   const bool snapped =
      search.goal.direction < kDirections && at<uint8_t>(gameViewObject, layout.gameViewBuildDirection) >= kDirections;
   std::string said = summary(drawn, search, snapped);
   const bool asked = g_sayRoute.exchange(false);
   if (said == state.summary && !asked) return;
   state.summary = said;
   speech::say(std::move(said));
}

// NamedBool<Tag>, BuildMode and EntitySelectionMode are one byte each, all by value; the
// std::function goes by address, and the callee destroys it.
using DrawPlanFunction = bool (*)(const void* controller, const void* cursor, void* queue, uint8_t ghost,
                                  uint8_t buildMode, const void* planner, void* entityGetter, uint8_t selectionMode);
DrawPlanFunction g_drawPlanOriginal = nullptr;

bool detourDrawPlan(const void* controller, const void* cursor, void* queue, uint8_t ghost, uint8_t buildMode,
                    const void* planner, void* entityGetter, uint8_t selectionMode) {
   if (!world::drivesCursor() || t_drawn)
      return g_drawPlanOriginal(controller, cursor, queue, ghost, buildMode, planner, entityGetter, selectionMode);
   // A goal pinned by the detach key (cursor.h): the route goes there, and snaps to the rail end
   // there, not to the one the free cursor selects. The game takes the entity from the getter when
   // it holds one, else from its selection; MSVC's std::function is the game's, and the callee
   // destroys whichever it is given.
   using Getter = std::function<const void*()>;
   Getter pinnedGetter;
   // A MapPosition: two fixed point ints.
   struct Point {
      int32_t x;
      int32_t y;
   } pinnedAt{};
   if (const auto pin = world::pinnedBuild()) {
      std::destroy_at(static_cast<Getter*>(entityGetter));
      pinnedAt = {pin->x, pin->y};
      pinnedGetter = [pinnedAt] { return reader::selectedAtPoint(pinnedAt.x, pinnedAt.y); };
      entityGetter = &pinnedGetter;
      cursor = &pinnedAt;
   }
   Drawn drawn;
   struct Scope {
      explicit Scope(Drawn* drawn) { t_drawn = drawn; }
      ~Scope() { t_drawn = nullptr; }
   };
   bool result;
   {
      Scope scope(&drawn);
      result = g_drawPlanOriginal(controller, cursor, queue, ghost, buildMode, planner, entityGetter, selectionMode);
   }
   update(drawn);
   return result;
}

using DrawArrowFunction = void (*)(const void* end, void* queue, uint8_t anchored, bool tooFar);
DrawArrowFunction g_drawArrowOriginal = nullptr;

void detourDrawArrow(const void* end, void* queue, uint8_t anchored, bool tooFar) {
   if (t_drawn) {
      t_drawn->arrow = true;
      t_drawn->end = location(end);
      t_drawn->tooFar = tooFar;
   }
   g_drawArrowOriginal(end, queue, anchored, tooFar);
}

// RenderUtil::Corner is an enum, by value.
using DrawCountsFunction = void (*)(void* queue, const void* counts, int32_t corner, const void* color);
DrawCountsFunction g_drawCountsOriginal = nullptr;

void detourDrawCounts(void* queue, const void* counts, int32_t corner, const void* color) {
   if (t_drawn) t_drawn->counts = readCounts(counts);
   g_drawCountsOriginal(queue, counts, corner, color);
}

// RailBuildPlan is returned through a hidden pointer, which the call gives back.
using BestSolutionFunction = void* (*)(void* finder, void* plan);
BestSolutionFunction g_bestSolutionOriginal = nullptr;

void* detourBestSolution(void* finder, void* plan) {
   void* result = g_bestSolutionOriginal(finder, plan);
   if (t_drawn) {
      const auto* self = static_cast<const std::byte*>(result);
      t_drawn->planned = true;
      t_drawn->planStart = location(self + layout.railBuildPlanStart);
      const auto& rails = at<MsvcVector<const std::byte>>(self, layout.railBuildPlanRails);
      t_drawn->rails.clear();
      for (const std::byte* rail = rails.first; rail < rails.last; rail += layout.locatedEntitySize)
         t_drawn->rails.push_back(
            {at<uint16_t>(rail, layout.locatedEntityId), at<int32_t>(rail, layout.locatedEntityPosition),
             at<int32_t>(rail, layout.locatedEntityPosition + 4), at<uint8_t>(rail, layout.locatedEntityDirection)});
   }
   return result;
}

} // namespace

bool drawing() { return t_drawn != nullptr; }

void sayRoute() { g_sayRoute = true; }

std::optional<Route> route() {
   const std::lock_guard lock(g_routeMutex);
   if (std::chrono::steady_clock::now() - g_routeTime > std::chrono::milliseconds(500)) return std::nullopt;
   return g_route;
}

void* drawPlanDetour() { return reinterpret_cast<void*>(&detourDrawPlan); }
void** drawPlanOriginal() { return reinterpret_cast<void**>(&g_drawPlanOriginal); }
void* drawArrowDetour() { return reinterpret_cast<void*>(&detourDrawArrow); }
void** drawArrowOriginal() { return reinterpret_cast<void**>(&g_drawArrowOriginal); }
void* drawCountsDetour() { return reinterpret_cast<void*>(&detourDrawCounts); }
void** drawCountsOriginal() { return reinterpret_cast<void**>(&g_drawCountsOriginal); }
void* bestSolutionDetour() { return reinterpret_cast<void*>(&detourBestSolution); }
void** bestSolutionOriginal() { return reinterpret_cast<void**>(&g_bestSolutionOriginal); }

} // namespace fa::railplanner
