#include "selection.h"

#include "game.h"
#include "speech.h"
#include "text.h"
#include "vocab.h"
#include "world.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace fa::selection {

namespace {

using game::layout;

template <class T>
T at(const void* object, uint32_t offset) {
   T value;
   std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
   return value;
}

// SelectionMode: Nothing (no selection is open), Select, AlternativeSelect (Shift),
// SuperForcedSelect, ReverseSelect.
constexpr int32_t kNothing = 0;
constexpr int32_t kSelect = 1;
constexpr int32_t kAlternativeSelect = 2;
constexpr int32_t kReverseSelect = 4;

// What the open selection's counts are of, and the counts as said, from its last frame drawn.
enum class CountsKind { Build, Deconstruction, Upgrade };
std::mutex g_countsMutex;
CountsKind g_countsKind = CountsKind::Build;
std::vector<std::string> g_counts;

// Whether the local player's controller applies a planner's selection at once rather than marking
// (Controller::getInstantDeconstruction or getInstantUpgrading at `slot`): the map editor's settings.
bool instant(std::byte* view, uint32_t slot) {
   const auto* player = at<const std::byte*>(view, layout.gameViewPlayer);
   const auto* controller = player ? at<const std::byte*>(player, layout.playerController) : nullptr;
   if (!controller) return false;
   using Getter = bool (*)(const void*);
   return (*reinterpret_cast<const Getter* const*>(controller))[slot](controller);
}

// Says what a deconstruction or upgrade planner's finished selection did, then the counts it showed
// last; nothing for a selection that took in nothing. A copy or blueprint's result is said where it
// lands: the item in hand, the blueprint's window. `view` is the GameView it was open in.
void sayFinished(std::byte* view, int32_t mode) {
   CountsKind kind;
   std::vector<std::string> counts;
   {
      std::scoped_lock lock(g_countsMutex);
      kind = g_countsKind;
      counts = std::move(g_counts);
      g_counts.clear();
   }
   if (counts.empty()) return;
   const vocab::Word* action = nullptr;
   if (kind == CountsKind::Deconstruction && mode == kSelect)
      action = instant(view, layout.controllerInstantDeconstruction) ? &vocab::kSelectionDeconstructed
                                                                     : &vocab::kSelectionDeconstruct;
   else if (kind == CountsKind::Deconstruction && mode == kAlternativeSelect)
      action = &vocab::kSelectionCancelDeconstruct;
   else if (kind == CountsKind::Upgrade && (mode == kSelect || mode == kReverseSelect))
      action =
         instant(view, layout.controllerInstantUpgrading) ? &vocab::kSelectionUpgraded : &vocab::kSelectionUpgrade;
   else if (kind == CountsKind::Upgrade && mode == kAlternativeSelect)
      action = &vocab::kSelectionCancelUpgrade;
   if (!action) return;
   std::string said = *action;
   for (const std::string& count : counts) said += ", " + count;
   speech::say(std::move(said), true);
}

// This client's GameView, which holds the open selection; null outside a game.
std::byte* gameView() {
   auto* context = *reinterpret_cast<std::byte**>(layout.globalContext);
   if (!context) return nullptr;
   auto* game = *reinterpret_cast<std::byte**>(context + layout.globalGame);
   return game ? *reinterpret_cast<std::byte**>(game + layout.gameView) : nullptr;
}

int32_t& selectionMode(std::byte* view) { return *reinterpret_cast<int32_t*>(view + layout.gameViewSelectionMode); }

// The GameView with a selection open while the mod drives the cursor, else null.
std::byte* openSelection() {
   if (!world::drivesCursor()) return nullptr;
   std::byte* view = gameView();
   return view && selectionMode(view) != kNothing ? view : nullptr;
}

// Closes the selection without its action, as the game does when the player changes surface.
void cancel(std::byte* view) {
   *reinterpret_cast<int32_t*>(view + layout.gameViewStartSelectionMode) = kNothing;
   selectionMode(view) = kNothing;
   *reinterpret_cast<uint32_t*>(view + layout.gameViewSelectionSurface) = UINT32_MAX;
   // An empty Optional<MapPosition> has INT32_MAX in both coordinates.
   *reinterpret_cast<int32_t*>(view + layout.gameViewSelectionPosition) = INT32_MAX;
   *reinterpret_cast<int32_t*>(view + layout.gameViewSelectionPosition + 4) = INT32_MAX;
   *reinterpret_cast<int64_t*>(view + layout.gameViewSelectionStartTime) = 0;
}

using ExpectedModeFunction = int32_t (*)(const void* source, bool custom);
ExpectedModeFunction g_expectedModeOriginal = nullptr;

// With no select control held the mouse finishes the selection. Here, as with a gamepad, it stays
// open in the mode it has.
int32_t detourExpectedMode(const void* source, bool custom) {
   const int32_t mode = g_expectedModeOriginal(source, custom);
   if (mode != kNothing) return mode;
   std::byte* view = openSelection();
   return view ? selectionMode(view) : kNothing;
}

using SelectionToolFunction = bool (*)(void* source, int32_t mode, int32_t customMode);
SelectionToolFunction g_selectionToolOriginal = nullptr;
using FinishSelectionFunction = void (*)(void* source);

// A select press while a selection is open finishes it, as the game does for a gamepad.
bool detourSelectionTool(void* source, int32_t mode, int32_t customMode) {
   if (std::byte* view = openSelection()) {
      const int32_t open = selectionMode(view);
      reinterpret_cast<FinishSelectionFunction>(layout.finishSelection)(source);
      sayFinished(view, open);
      return true;
   }
   return g_selectionToolOriginal(source, mode, customMode);
}

using DeselectionFunction = bool (*)(void* source);
DeselectionFunction g_deselectionOriginal = nullptr;

// The deselect control's second press finishes its selection too.
bool detourDeselection(void* source) {
   if (std::byte* view = openSelection()) {
      const int32_t open = selectionMode(view);
      reinterpret_cast<FinishSelectionFunction>(layout.finishSelection)(source);
      sayFinished(view, open);
      return true;
   }
   return g_deselectionOriginal(source);
}

using TriggeredByFunction = const void* (*)(const void* control, const void* event, const void* context, uint32_t);

bool triggersToggleMenu(const void* event) {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   const auto* settings = *reinterpret_cast<const std::byte* const*>(context + layout.globalControlSettings);
   // ControlContext: inGui, false in the world.
   const bool controlContext = false;
   return reinterpret_cast<TriggeredByFunction>(layout.controlTriggeredBy)(settings + layout.controlSettingsToggleMenu,
                                                                           event, &controlContext, 0) != nullptr;
}

using ProcessActionsFunction = bool (*)(void* source, const void* event, bool paused);
ProcessActionsFunction g_processActionsOriginal = nullptr;

// Escape reaches a dozen handlers (closing a dropdown, a window, remote view, then the game menu)
// before it would reach a selection, so the cancel comes first.
bool detourProcessActions(void* source, const void* event, bool paused) {
   if (std::byte* view = openSelection(); view && triggersToggleMenu(event)) {
      cancel(view);
      speech::say(std::string(vocab::kSelectionCancelled), true);
      return true;
   }
   return g_processActionsOriginal(source, event, paused);
}

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

template <class T>
struct MsvcVector {
   T* first;
   T* last;
   T* end;
};

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

// An IDWithQuality's name as the game would label it: the quality first, unless it is normal.
std::string name(uintptr_t prototypes, const std::byte* id) {
   const std::byte* prototype = prototypeAt(prototypes, at<uint16_t>(id, layout.idWithQualityBase));
   if (!prototype) return {};
   std::string said = localisedName(prototype);
   const std::byte* quality = prototypeAt(layout.qualityPrototypes, at<uint8_t>(id, layout.idWithQualityQuality));
   if (quality && view(at<MsvcString>(quality, layout.prototypeName)) != "normal")
      said = localisedName(quality) + " " + said;
   return said;
}

// The node of a std::map (MSVC): left, parent, right, colour, end marker, then the stored pair.
struct TreeNode {
   TreeNode* left;
   TreeNode* parent;
   TreeNode* right;
   char colour;
   char isNil;
};

template <class Visit>
void forEachNode(const std::byte* map, Visit visit) {
   auto* head = at<TreeNode*>(map, 0);
   std::vector<const TreeNode*> pending{head->parent};
   while (!pending.empty()) {
      const TreeNode* node = pending.back();
      pending.pop_back();
      if (node->isNil) continue;
      visit(reinterpret_cast<const std::byte*>(node));
      pending.push_back(node->left);
      pending.push_back(node->right);
   }
}

bool empty(const std::byte* map) { return at<size_t>(map, sizeof(void*)) == 0; }

struct Line {
   uint32_t count;
   std::string text;
};

// "12 transport belt" for each entry of a map from IDWithQuality to a count.
void addCounts(std::vector<Line>& lines, const std::byte* map, uintptr_t prototypes) {
   forEachNode(map, [&](const std::byte* node) {
      const auto count = at<uint32_t>(node, layout.countNodeCount);
      if (count == 0) return;
      lines.push_back({count, std::format("{} {}", count, name(prototypes, node + layout.countNodeId))});
   });
}

// "3 transport belt to fast transport belt" for each entry of a map from a pair of IDWithQuality.
void addUpgrades(std::vector<Line>& lines, const std::byte* map, uintptr_t prototypes) {
   forEachNode(map, [&](const std::byte* node) {
      const auto count = at<uint32_t>(node, layout.upgradeNodeCount);
      if (count == 0) return;
      lines.push_back({count, vocab::kUpgrade(count, name(prototypes, node + layout.upgradeNodeFrom),
                                              name(prototypes, node + layout.upgradeNodeTo))});
   });
}

// What the counts the game draws beside the box are of.
CountsKind countsKind(const std::byte* renderer) {
   if (at<bool>(renderer, layout.selectionRendererDeconstruction)) return CountsKind::Deconstruction;
   const std::byte* counts = renderer + layout.selectionRendererCounts;
   return empty(counts + layout.countsItemsToBuild) ? CountsKind::Upgrade : CountsKind::Build;
}

// The counts the game draws beside the box, largest first, as SelectionToolRenderer::
// drawSelectionCounts and RenderUtil::drawDeconstructionCounts choose them.
std::vector<Line> counts(const std::byte* renderer) {
   const std::byte* counts = renderer + layout.selectionRendererCounts;
   std::vector<Line> lines;
   auto sorted = [&](size_t from) {
      std::stable_sort(lines.begin() + static_cast<ptrdiff_t>(from), lines.end(),
                       [](const Line& a, const Line& b) { return a.count > b.count; });
   };
   const CountsKind kind = countsKind(renderer);
   if (kind == CountsKind::Deconstruction) {
      addCounts(lines, counts + layout.countsEntities, layout.entityPrototypes);
      sorted(0);
      const size_t items = lines.size();
      addCounts(lines, counts + layout.countsItemsNotToBuild, layout.itemPrototypes);
      sorted(items);
   } else if (kind == CountsKind::Build) {
      addCounts(lines, counts + layout.countsItemsToBuild, layout.itemPrototypes);
      sorted(0);
   } else {
      addUpgrades(lines, counts + layout.countsEntityUpgrades, layout.entityPrototypes);
      addUpgrades(lines, counts + layout.countsItemUpgrades, layout.itemPrototypes);
      sorted(0);
   }
   return lines;
}

// The selection said last: its start time, and the tile the cursor corner was on.
std::mutex g_saidMutex;
int64_t g_saidSelection = 0;
int32_t g_saidX = 0;
int32_t g_saidY = 0;

using DrawCountsFunction = void (*)(void* renderer, void* queue, const void* color);
DrawCountsFunction g_drawCountsOriginal = nullptr;

// Each time the cursor corner moves to another tile, the box's size in tiles and what it takes in,
// after what the mod says of the move.
void detourDrawCounts(void* renderer, void* queue, const void* color) {
   g_drawCountsOriginal(renderer, queue, color);
   std::byte* view = openSelection();
   if (!view) return;
   const auto* self = static_cast<const std::byte*>(renderer);
   const std::vector<Line> lines = counts(self);
   {
      std::scoped_lock lock(g_countsMutex);
      g_countsKind = countsKind(self);
      g_counts.clear();
      for (const Line& line : lines) g_counts.push_back(line.text);
   }
   const auto selection = at<int64_t>(view, layout.gameViewSelectionStartTime);
   // MapPositions are fixed point, 256 to the tile.
   const int32_t cursorX = at<int32_t>(self, layout.selectionRendererCursor) >> 8;
   const int32_t cursorY = at<int32_t>(self, layout.selectionRendererCursor + 4) >> 8;
   {
      std::lock_guard lock(g_saidMutex);
      const bool moved = selection == g_saidSelection && (cursorX != g_saidX || cursorY != g_saidY);
      g_saidSelection = selection;
      g_saidX = cursorX;
      g_saidY = cursorY;
      if (!moved) return;
   }
   const int32_t width = std::abs(cursorX - (at<int32_t>(self, layout.selectionRendererStart) >> 8)) + 1;
   const int32_t height = std::abs(cursorY - (at<int32_t>(self, layout.selectionRendererStart + 4) >> 8)) + 1;
   std::string said = vocab::kBoxSize(width, height);
   for (const Line& line : lines) said += ", " + line.text;
   speech::say(std::move(said), false);
}

} // namespace

void* expectedModeDetour() { return reinterpret_cast<void*>(&detourExpectedMode); }
void** expectedModeOriginal() { return reinterpret_cast<void**>(&g_expectedModeOriginal); }
void* selectionToolDetour() { return reinterpret_cast<void*>(&detourSelectionTool); }
void** selectionToolOriginal() { return reinterpret_cast<void**>(&g_selectionToolOriginal); }
void* deselectionDetour() { return reinterpret_cast<void*>(&detourDeselection); }
void** deselectionOriginal() { return reinterpret_cast<void**>(&g_deselectionOriginal); }
void* processActionsDetour() { return reinterpret_cast<void*>(&detourProcessActions); }
void** processActionsOriginal() { return reinterpret_cast<void**>(&g_processActionsOriginal); }
void* drawCountsDetour() { return reinterpret_cast<void*>(&detourDrawCounts); }
void** drawCountsOriginal() { return reinterpret_cast<void**>(&g_drawCountsOriginal); }

} // namespace fa::selection
