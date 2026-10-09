#include "chart.h"

#include "game.h"
#include "speech.h"
#include "text.h"
#include "vocab.h"
#include "world.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

namespace fa::chart {

namespace {

using game::layout;

template <class T>
T at(const void* object, uint32_t offset) {
   T value;
   std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
   return value;
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

// Frees what a std::string the game returned holds, as its own destructor does: through the game's
// allocator, which keeps the true start of a large block just before it.
void destroy(MsvcString& string) {
   if (string.capacity < sizeof(string.buffer)) return;
   void* block = string.pointer;
   if (string.capacity + 1 >= 0x1000) block = reinterpret_cast<void**>(block)[-1];
   reinterpret_cast<void (*)(void*)>(layout.gameOperatorDelete)(block);
}

const std::byte* currentGame() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   return context ? at<const std::byte*>(context, layout.globalGame) : nullptr;
}

// This client's PlayerInputSource, when its player is the current game's local player and shows
// the full map. The game's view must show that player too: getChartSelection reads it, and aborts
// the game without it, as just after a save loads.
const std::byte* chartSource() {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   const std::byte* game = currentGame();
   if (!context || !game) return nullptr;
   const std::byte* source = at<const std::byte*>(context, layout.globalPlayerInputSource);
   const std::byte* player = at<const std::byte*>(game, layout.gameLocalPlayer);
   if (!source || !player || at<const std::byte*>(source, layout.inputSourcePlayer) != player) return nullptr;
   const std::byte* view = at<const std::byte*>(game, layout.gameView);
   if (!view || at<const std::byte*>(view, layout.gameViewPlayer) != player) return nullptr;
   return at<uint8_t>(player, layout.playerRenderMode) == game::kRenderModeChart ? source : nullptr;
}

const std::byte* sourcePlayer(const std::byte* source) { return at<const std::byte*>(source, layout.inputSourcePlayer); }

using LocalisedStr = const MsvcString* (*)(const void* localisedString, const void* localeProvider);

std::string prototypeName(const std::byte* prototype) {
   const MsvcString* name =
      reinterpret_cast<LocalisedStr>(layout.localisedStringStr)(prototype + layout.prototypeLocalisedName, nullptr);
   return text::speakable(view(*name));
}

using SelectionFunction = void* (*)(const void* source, std::byte* out);
using PatchConstructFunction = void* (*)(void* info, bool useClockLimiter);
using PatchDestroyFunction = void (*)(void* info);
using PatchUpdateFunction = bool (*)(void* info, const void* resource, const void* force, bool keepIfUnchanged);
using FormattedNameFunction = MsvcString* (*)(MsvcString* out, const void* material, double amount,
                                              const void* prototype);

// The patch finder, kept across frames so that pointing again into the same patch costs nothing.
// It belongs to the game it was made in.
alignas(16) std::byte g_patchInfo[game::kPatchInfoCapacity];
const void* g_patchGame = nullptr;

void* patchInfo(const std::byte* game) {
   if (g_patchGame == game) return g_patchInfo;
   if (g_patchGame) reinterpret_cast<PatchDestroyFunction>(layout.patchInfoDestroy)(g_patchInfo);
   reinterpret_cast<PatchConstructFunction>(layout.patchInfoConstruct)(g_patchInfo, false);
   g_patchGame = game;
   return g_patchInfo;
}

// The node of a std::map (MSVC): left, parent, right, colour, end marker, then the value.
struct TreeNode {
   TreeNode* left;
   TreeNode* parent;
   TreeNode* right;
   char colour;
   char isNil;
};
constexpr uint32_t kTreeValue = 0x20;

TreeNode* nextNode(TreeNode* node) {
   if (!node->right->isNil) {
      node = node->right;
      while (!node->left->isNil) node = node->left;
      return node;
   }
   TreeNode* parent = node->parent;
   while (!parent->isNil && node == parent->right) {
      node = parent;
      parent = parent->parent;
   }
   return parent;
}

std::string labelOf(const void* info, const std::byte* resource) {
   const void* prototype = at<const void*>(info, layout.patchInfoPrototype);
   auto* head = at<TreeNode*>(info, layout.patchInfoCounts);
   std::string label;
   const uint32_t amountOffset = kTreeValue + (layout.materialIdSize + 7) / 8 * 8;
   for (TreeNode* node = head->left; node != head; node = nextNode(node)) {
      const auto* bytes = reinterpret_cast<const std::byte*>(node);
      MsvcString line{};
      reinterpret_cast<FormattedNameFunction>(layout.patchFormattedName)(&line, bytes + kTreeValue,
                                                                         at<double>(bytes, amountOffset), prototype);
      if (!label.empty()) label += ", ";
      label += text::speakable(view(line));
      destroy(line);
   }
   if (label.empty()) label = prototypeName(at<const std::byte*>(resource, layout.entityPrototypeOf));
   return label;
}

// The map's label of the patch `resource` is in.
std::string describePatch(const std::byte* player, const std::byte* resource) {
   const std::byte* map = at<const std::byte*>(player, layout.playerMap);
   const auto* forces = at<const std::byte* const*>(map, layout.mapForceData);
   const std::byte* force = forces[at<uint8_t>(player, layout.playerForce)];
   void* info = patchInfo(currentGame());
   reinterpret_cast<PatchUpdateFunction>(layout.patchInfoUpdate)(info, resource, force, true);
   return labelOf(info, resource);
}

std::string describe(const std::byte* player, const std::byte* selection) {
   if (const auto* tag = at<const std::byte*>(selection, layout.chartSelectionTag)) {
      std::string text = text::speakable(view(at<MsvcString>(tag, layout.chartTagText)));
      return text.empty() ? std::string(vocab::kMapTag) : text + ", " + std::string(vocab::kMapTag);
   }
   if (const auto* target = at<const std::byte*>(selection, layout.chartSelectionTarget))
      return prototypeName(at<const std::byte*>(target, layout.entityPrototypeOf));
   if (const auto* resource = at<const std::byte*>(selection, layout.chartSelectionPatch))
      return describePatch(player, resource);
   return {};
}

// The selection said last; a new one is said once, however the cursor moves within it.
const void* g_selected = nullptr;
std::string g_said;
// Where the cursor was when something was last selected. A selection tool in hand hides the
// selection, and it comes back unchanged when the tool leaves the hand with the cursor still there.
std::optional<world::CursorPosition> g_selectedAt;

} // namespace

std::string patchLabel(const void* info, const void* resource) {
   return labelOf(info, static_cast<const std::byte*>(resource));
}

void tick() {
   const std::optional<world::CursorPosition> cursor = world::cursorPosition();
   const std::byte* source = cursor ? chartSource() : nullptr;
   alignas(8) std::byte selection[game::kChartSelectionCapacity]{};
   if (source) reinterpret_cast<SelectionFunction>(layout.chartSelection)(source, selection);
   const void* tag = at<const void*>(selection, layout.chartSelectionTag);
   const void* target = at<const void*>(selection, layout.chartSelectionTarget);
   const void* selected = tag ? tag : target ? target : at<const void*>(selection, layout.chartSelectionPatch);
   if (selected)
      g_selectedAt = cursor;
   else if (cursor != g_selectedAt)
      g_said.clear();
   if (selected == g_selected) return;
   g_selected = selected;
   if (!selected) return;
   // Another resource of the same patch reads the same.
   std::string said = describe(sourcePlayer(source), selection);
   if (said.empty() || said == g_said) return;
   g_said = said;
   speech::say(std::move(said), false);
}

} // namespace fa::chart
