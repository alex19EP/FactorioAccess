#include "skip.h"

#include "facts.h"
#include "reader.h"

namespace fa::skip {

namespace {

// The world as the game shows it, for skiprules.h.
struct GameWorld {
   using Entity = const void*;
   Entity selectedAt(int32_t x, int32_t y) const { return reader::selectedAt(x, y); }
   bool waterAt(int32_t x, int32_t y) const { return reader::waterAt(x, y); }
   std::optional<reader::Shown> shownAt(int32_t x, int32_t y) const { return reader::shownAt(x, y); }
   bool skipsOver(Entity entity) const { return facts::skipsOver(entity); }
   facts::Likeness likenessOf(Entity entity) const { return facts::likenessOf(entity); }
   std::optional<facts::UndergroundEnd> undergroundEnd(Entity entity, uint8_t direction) const {
      return facts::undergroundEnd(entity, direction);
   }
};

} // namespace

Result find(int32_t x, int32_t y, int32_t dx, int32_t dy, int limit,
            const std::function<bool(int32_t, int32_t)>& allowed, std::span<const Ruler> rulers) {
   return skip::find(GameWorld{}, x, y, dx, dy, limit, allowed, rulers);
}

} // namespace fa::skip
