#include "entityviews.h"

#include "log.h"

#include <mutex>
#include <utility>

namespace fa::entityviews {

namespace {

std::mutex g_mutex;
Views g_pending;
std::shared_ptr<const Views> g_current;

} // namespace

void begin(uint64_t unitNumber) {
   std::lock_guard lock(g_mutex);
   g_pending = Views{unitNumber, {}};
}

void addView(std::string title) {
   std::lock_guard lock(g_mutex);
   g_pending.views.push_back(View{std::move(title), {}});
}

void addColumn(std::string title, std::vector<std::string> cells) {
   std::lock_guard lock(g_mutex);
   if (g_pending.views.empty()) {
      log::error("entity view column \"{}\" sent before any view", title);
      return;
   }
   g_pending.views.back().columns.push_back(Column{std::move(title), std::move(cells)});
}

void end() {
   std::lock_guard lock(g_mutex);
   g_current = std::make_shared<const Views>(std::move(g_pending));
   g_pending = {};
}

std::shared_ptr<const Views> current() {
   std::lock_guard lock(g_mutex);
   return g_current;
}

} // namespace fa::entityviews
