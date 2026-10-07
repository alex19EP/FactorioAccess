#include "parts.h"

#include <atomic>
#include <iterator>

namespace fa::parts {

namespace {

// In the order Ctrl+Tab visits them, None first.
constexpr Part kOrder[] = {Part::None, Part::QuickBar};
constexpr int kCount = static_cast<int>(std::size(kOrder));

// Set from Lua on the game's update and from the navigator on the Gui's logic.
std::atomic<Part> g_current{Part::None};

int indexOf(Part part) {
   for (int i = 0; i < kCount; ++i)
      if (kOrder[i] == part) return i;
   return 0;
}

} // namespace

Part current() { return g_current.load(); }

void cycle(int direction) {
   int step = direction < 0 ? kCount - 1 : 1;
   Part from = g_current.load();
   while (!g_current.compare_exchange_weak(from, kOrder[(indexOf(from) + step) % kCount])) {
   }
}

void close() { g_current.store(Part::None); }

} // namespace fa::parts
