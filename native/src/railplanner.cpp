#include "railplanner.h"

#include "game.h"
#include "speech.h"
#include "text.h"
#include "vocab.h"
#include "world.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fa::railplanner {

namespace {

using game::layout;

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

// What one drawRailPlanBuilder call of this client drew: the route's end arrow and its counts.
struct Drawn {
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
};
State g_state;

// The prototype an ID indexes in a PrototypeList<T>::indexToPrototype vector, or null.
const std::byte* prototypeAt(uintptr_t list, size_t index) {
   const auto& prototypes = *reinterpret_cast<const MsvcVector<const std::byte* const>*>(list);
   return index < static_cast<size_t>(prototypes.last - prototypes.first) ? prototypes.first[index] : nullptr;
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

// The route as the game shows it once its search has stopped.
std::string summary(const Drawn& drawn, const Location& goal) {
   if (!drawn.arrow) return vocab::kRailPlanNoRoute;
   std::string said;
   for (const Count& count : drawn.counts) {
      if (count.count == 0) continue;
      if (!said.empty()) said += ", ";
      said += countText(count);
   }
   if (!said.empty()) said += ", ";
   said += vocab::kRailPlanEnds(vocab::kDirection(drawn.end.direction));
   // A route the game could not take all the way stops short of the cursor. A rail end lies on the
   // two-tile rail grid, so one that reaches the cursor is within two tiles of it.
   const int32_t shortBy =
      std::max(std::abs(drawn.end.x - goal.x), std::abs(drawn.end.y - goal.y)) / game::kMapPositionScale;
   if (shortBy > 2) said += ", " + vocab::kRailPlanShort(shortBy);
   if (drawn.tooFar) said += ", " + std::string(vocab::kOutOfReach);
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
      if (state.active) speech::say(std::string(vocab::kRailPlanEnded), false);
      state.active = false;
      state.spoken.reset();
      state.lastEnd.reset();
      return;
   }
   const std::byte* finder = at<const std::byte*>(gameViewObject, layout.gameViewRailPlanFinder);
   if (!finder) return;
   const Search search{location(gameViewObject + layout.gameViewRailPlanStart),
                       location(finder + layout.railPlanFinderGoal),
                       at<bool>(finder, layout.railPlanFinderGhostMode)};
   const auto steps = at<uint32_t>(finder, layout.railPlanFinderStepCount);
   const std::optional<Location> lastEnd = state.lastEnd;
   state.lastEnd = drawn.arrow ? std::optional(drawn.end) : std::nullopt;

   if (!state.active || search.start != state.start) {
      const bool built = state.active && lastEnd && *lastEnd == search.start;
      const std::string direction = vocab::kDirection(search.start.direction);
      speech::say(built ? vocab::kRailPlanBuilt(direction) : vocab::kRailPlanStart(direction), false);
      state.active = true;
      state.start = search.start;
      // The cursor is still where planning started; the route to it is said once it moves.
      state.spoken = search;
      state.summary.clear();
      state.steps = steps;
      return;
   }
   // The search runs a few hundred steps a frame and stops at a solution, a dead end or its limit.
   const bool stopped = steps == state.steps;
   state.steps = steps;
   if (!stopped || state.spoken == search) return;
   state.spoken = search;
   std::string said = summary(drawn, search.goal);
   if (said == state.summary) return;
   state.summary = said;
   speech::say(std::move(said), false);
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

} // namespace

bool drawing() { return t_drawn != nullptr; }

void* drawPlanDetour() { return reinterpret_cast<void*>(&detourDrawPlan); }
void** drawPlanOriginal() { return reinterpret_cast<void**>(&g_drawPlanOriginal); }
void* drawArrowDetour() { return reinterpret_cast<void*>(&detourDrawArrow); }
void** drawArrowOriginal() { return reinterpret_cast<void**>(&g_drawArrowOriginal); }
void* drawCountsDetour() { return reinterpret_cast<void*>(&detourDrawCounts); }
void** drawCountsOriginal() { return reinterpret_cast<void**>(&g_drawCountsOriginal); }

} // namespace fa::railplanner
