#include "parts.h"

#include <atomic>
#include <iterator>

namespace fa::parts {

namespace {

// In the order Ctrl+Tab visits them, None first.
constexpr Part kOrder[] = {Part::None,        Part::ModWindows,    Part::QuickBar,
                           Part::ShortcutBar, Part::SideMenu,      Part::MapViewOptions,
                           Part::Status,      Part::CraftingQueue, Part::TrackedAchievements};
constexpr int kCount = static_cast<int>(std::size(kOrder));

// Set from Lua on the game's update and from the navigator on the Gui's logic.
std::atomic<Part> g_current{Part::None};

// The only part that is not always there; it waits for its screen to see a window.
std::atomic<bool> g_modWindows{false};

bool available(Part part) { return part != Part::ModWindows || g_modWindows.load(); }

int indexOf(Part part) {
   for (int i = 0; i < kCount; ++i)
      if (kOrder[i] == part) return i;
   return 0;
}

// The part after `from` in the direction of `step`, passing over those not available. None always
// is, so the search ends.
Part next(Part from, int step) {
   int index = indexOf(from);
   do index = (index + step) % kCount;
   while (!available(kOrder[index]));
   return kOrder[index];
}

} // namespace

Part current() { return g_current.load(); }

void cycle(int direction) {
   int step = direction < 0 ? kCount - 1 : 1;
   Part from = g_current.load();
   while (!g_current.compare_exchange_weak(from, next(from, step))) {
   }
}

void setAvailable(Part part, bool available) {
   if (part == Part::ModWindows) g_modWindows = available;
}

void close() { g_current.store(Part::None); }

} // namespace fa::parts
