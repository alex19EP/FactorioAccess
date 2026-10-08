#include "selection.h"

#include "game.h"
#include "speech.h"
#include "vocab.h"
#include "world.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace fa::selection {

namespace {

using game::layout;

// SelectionMode::Nothing: no selection is open.
constexpr int32_t kNothing = 0;

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
   if (openSelection()) {
      reinterpret_cast<FinishSelectionFunction>(layout.finishSelection)(source);
      return true;
   }
   return g_selectionToolOriginal(source, mode, customMode);
}

using TriggeredByFunction = const void* (*)(const void* control, const void* event, const void* context,
                                            uint32_t);

bool triggersToggleMenu(const void* event) {
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   const auto* settings = *reinterpret_cast<const std::byte* const*>(context + layout.globalControlSettings);
   // ControlContext: inGui, false in the world.
   const bool controlContext = false;
   return reinterpret_cast<TriggeredByFunction>(layout.controlTriggeredBy)(
              settings + layout.controlSettingsToggleMenu, event, &controlContext, 0) != nullptr;
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

} // namespace

void* expectedModeDetour() { return reinterpret_cast<void*>(&detourExpectedMode); }
void** expectedModeOriginal() { return reinterpret_cast<void**>(&g_expectedModeOriginal); }
void* selectionToolDetour() { return reinterpret_cast<void*>(&detourSelectionTool); }
void** selectionToolOriginal() { return reinterpret_cast<void**>(&g_selectionToolOriginal); }
void* processActionsDetour() { return reinterpret_cast<void*>(&detourProcessActions); }
void** processActionsOriginal() { return reinterpret_cast<void**>(&g_processActionsOriginal); }

} // namespace fa::selection
