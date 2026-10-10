#include "mappicture.h"

#include "game.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace fa::mappicture {

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

struct MsvcString {
   union {
      char buffer[16];
      const char* pointer;
   };
   size_t size;
   size_t capacity;

   std::string_view view() const { return {capacity >= sizeof(buffer) ? pointer : buffer, size}; }
};

template <class T>
struct MsvcVector {
   T* first;
   T* last;
   T* end;
};

const std::byte* utility() {
   const auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   return context ? at<const std::byte*>(context, layout.globalUtilityConstants) : nullptr;
}

const MsvcVector<const std::byte* const>& prototypes(uintptr_t list) {
   return *reinterpret_cast<const MsvcVector<const std::byte* const>*>(list);
}

std::string_view typeName(const std::byte* prototype) {
   return virtualAt<const char* (*)(const void*)>(prototype, layout.prototypeGetType)(prototype);
}

bool hidden(const std::byte* prototype) { return at<bool>(prototype, layout.prototypeHidden); }

std::string_view order(const std::byte* prototype) {
   return reinterpret_cast<const MsvcString*>(prototype + layout.prototypeOrder)->view();
}

// std::optional<ByteColor> as the prototypes keep it: the colour, then whether there is one.
std::optional<ByteColor> optionalColour(const std::byte* prototype, uint32_t offset) {
   if (!at<bool>(prototype, offset + sizeof(ByteColor))) return std::nullopt;
   return at<ByteColor>(prototype, offset);
}

// Every colour the chart draws with, for the prototypes loaded now.
class Table {
public:
   void rebuildIfStale() {
      const auto& entities = prototypes(layout.entityPrototypes);
      const auto& tiles = prototypes(layout.tilePrototypes);
      if (entities.first == entityList_ && tiles.first == tileList_ && !colours_.empty()) return;
      entityList_ = entities.first;
      tileList_ = tiles.first;
      colours_.clear();
      const std::byte* constants = utility();
      if (!constants) return;

      for (const std::byte* const* it = tiles.first; it != tiles.last; ++it) {
         if (!*it) continue;
         colours_.addTile(at<ByteColor>(*it, layout.tilePrototypeMapColor), *it, hidden(*it), order(*it));
      }

      colours_.addKind(at<ByteColor>(constants, layout.chartDefaultFriendlyColor), Picture::Kind::Building);
      colours_.addKind(at<ByteColor>(constants, layout.chartDefaultEnemyColor), Picture::Kind::Enemy);
      colours_.addKind(at<ByteColor>(constants, layout.chartEntityGhostColor), Picture::Kind::Ghost);
      colours_.addKind(at<ByteColor>(constants, layout.chartTileGhostColor), Picture::Kind::TileGhost);

      for (const std::byte* const* it = entities.first; it != entities.last; ++it) {
         const std::byte* prototype = *it;
         if (!prototype || at<uint32_t>(prototype, layout.entityPrototypeFlags) & game::kEntityNotOnMap) continue;
         const std::string_view type = typeName(prototype);
         if (type == "entity-ghost" || type == "tile-ghost") continue;
         if (const auto enemy = optionalColour(prototype, layout.entityPrototypeEnemyMapColor))
            colours_.addKind(*enemy, Picture::Kind::Enemy);
         // Chart::drawEntity: the map colour, else the friendly one (EntityPrototype::getFriendlyMapColor).
         std::optional<ByteColor> colour = optionalColour(prototype, layout.entityPrototypeMapColor);
         if (!colour) colour = optionalColour(prototype, layout.entityPrototypeFriendlyMapColor);
         if (colour) colours_.addEntity(*colour, prototype, hidden(prototype), order(prototype));
      }
   }

   std::optional<Picture> lookup(uint16_t pixel) const { return colours_.lookup(pixel); }

private:
   const std::byte* const* entityList_ = nullptr;
   const std::byte* const* tileList_ = nullptr;
   Colours colours_;
};

Table g_table;

constexpr int32_t kChunkTiles = 32;

// The pixel the chart holds for the tile, or nothing where its chunk is not charted.
std::optional<uint16_t> pixelAt(const void* chart, int32_t tileX, int32_t tileY) {
   const auto* columns = static_cast<const std::byte*>(chart) + layout.chartColumns;
   const auto entry = [](const std::byte* vector, int32_t index) -> const std::byte* {
      const int32_t slot = index + at<int32_t>(vector, layout.infiniteZero);
      if (slot < 0 || slot >= at<int32_t>(vector, layout.infiniteSize)) return nullptr;
      return at<const std::byte*>(vector, layout.infiniteData) + static_cast<size_t>(slot) * layout.infiniteVectorSize;
   };
   // Floored, as chunks are.
   const int32_t chunkX = tileX >> 5, chunkY = tileY >> 5;
   const std::byte* column = entry(columns, chunkX);
   if (!column) return std::nullopt;
   const int32_t slot = chunkY + at<int32_t>(column, layout.infiniteZero);
   if (slot < 0 || slot >= at<int32_t>(column, layout.infiniteSize)) return std::nullopt;
   const auto* subChart = at<const std::byte* const*>(column, layout.infiniteData)[slot];
   if (!subChart) return std::nullopt;
   const int32_t x = tileX & (kChunkTiles - 1), y = tileY & (kChunkTiles - 1);
   return at<uint16_t>(subChart, layout.subChartPixels + static_cast<uint32_t>((y * kChunkTiles + x) * 2));
}

} // namespace

std::optional<Shown> at(const void* chart, int32_t tileX, int32_t tileY, const void* tile) {
   const std::optional<uint16_t> pixel = pixelAt(chart, tileX, tileY);
   if (!pixel) return std::nullopt;
   // Nothing was drawn there.
   if (*pixel == 0) return Shown{};
   if (tile && distanceBetween(pack(at<ByteColor>(tile, layout.tilePrototypeMapColor)), *pixel) == 0)
      return Shown{Picture{Picture::Kind::Tile, tile}};
   g_table.rebuildIfStale();
   return Shown{g_table.lookup(*pixel)};
}

} // namespace fa::mappicture
