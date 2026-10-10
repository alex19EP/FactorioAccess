#include "mappicture.h"

#include "game.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string_view>
#include <vector>

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

// ByteColor: r, g, b, a.
using ByteColor = std::array<uint8_t, 4>;

// A pixel's channels: 5 bits of red, 6 of green, 5 of blue.
struct Channels {
   int r;
   int g;
   int b;
};

uint16_t pack(ByteColor colour) {
   return static_cast<uint16_t>((colour[0] & 0xf8) << 8 | (colour[1] & 0xfc) << 3 | colour[2] >> 3);
}

Channels channels(uint16_t pixel) { return {pixel >> 11, pixel >> 5 & 0x3f, pixel & 0x1f}; }

// The pixel read back as eight bits a channel, as blending over it reads it.
ByteColor widen(uint16_t pixel) {
   const Channels c = channels(pixel);
   return {static_cast<uint8_t>(c.r << 3 | c.r >> 2), static_cast<uint8_t>(c.g << 2 | c.g >> 4),
           static_cast<uint8_t>(c.b << 3 | c.b >> 2), 0xff};
}

// ChartingInterface::drawRectangle: `colour` over `under` by its alpha.
ByteColor blend(ByteColor colour, ByteColor under) {
   const unsigned weight = colour[3] + (colour[3] >> 7), rest = 0x100 - weight;
   ByteColor out{};
   for (int i = 0; i < 3; ++i) out[i] = static_cast<uint8_t>((colour[i] * weight + under[i] * rest) >> 8);
   out[3] = 0xff;
   return out;
}

// One colour the chart draws with, and what draws it. On a tie the lower rank wins: the ground is
// more often what a pixel shows than a building of the same colour, and a translucent thing over a
// tile least often.
struct Entry {
   uint16_t pixel;
   Picture picture;
   int rank;
   bool hidden = false;
   std::string_view order;
};

int rankOf(Picture::Kind kind, bool blended) {
   if (blended) return 6;
   switch (kind) {
   case Picture::Kind::Ghost:
   case Picture::Kind::TileGhost: return 0;
   case Picture::Kind::Enemy: return 1;
   case Picture::Kind::Tile: return 2;
   case Picture::Kind::Entity: return 3;
   case Picture::Kind::Building: return 4;
   }
   return 5;
}

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
      if (entities.first == entityList_ && tiles.first == tileList_ && !entries_.empty()) return;
      entityList_ = entities.first;
      tileList_ = tiles.first;
      entries_.clear();
      const std::byte* constants = utility();
      if (!constants) return;

      std::vector<ByteColor> tileColours;
      for (const std::byte* const* it = tiles.first; it != tiles.last; ++it) {
         if (!*it) continue;
         const auto colour = at<ByteColor>(*it, layout.tilePrototypeMapColor);
         tileColours.push_back(colour);
         add(colour, {Picture::Kind::Tile, *it}, false, *it);
      }

      const auto friendly = at<ByteColor>(constants, layout.chartDefaultFriendlyColor);
      add(friendly, {Picture::Kind::Building, nullptr}, false, nullptr);
      add(at<ByteColor>(constants, layout.chartDefaultEnemyColor), {Picture::Kind::Enemy, nullptr}, false, nullptr);
      add(at<ByteColor>(constants, layout.chartEntityGhostColor), {Picture::Kind::Ghost, nullptr}, false, nullptr);
      add(at<ByteColor>(constants, layout.chartTileGhostColor), {Picture::Kind::TileGhost, nullptr}, false, nullptr);

      for (const std::byte* const* it = entities.first; it != entities.last; ++it) {
         const std::byte* prototype = *it;
         if (!prototype || at<uint32_t>(prototype, layout.entityPrototypeFlags) & game::kEntityNotOnMap) continue;
         const std::string_view type = typeName(prototype);
         if (type == "entity-ghost" || type == "tile-ghost") continue;
         if (const auto enemy = optionalColour(prototype, layout.entityPrototypeEnemyMapColor))
            add(*enemy, {Picture::Kind::Enemy, nullptr}, false, nullptr);
         // Chart::drawEntity: the map colour, else the friendly one (EntityPrototype::getFriendlyMapColor).
         std::optional<ByteColor> colour = optionalColour(prototype, layout.entityPrototypeMapColor);
         if (!colour) colour = optionalColour(prototype, layout.entityPrototypeFriendlyMapColor);
         if (!colour) continue;
         const Picture picture{Picture::Kind::Entity, prototype};
         if ((*colour)[3] == 0xff) {
            add(*colour, picture, false, prototype);
            continue;
         }
         // Drawn see-through over the ground: a tree's crown.
         for (const ByteColor under : tileColours) add(blend(*colour, widen(pack(under))), picture, true, prototype);
      }
   }

   std::optional<Picture> lookup(uint16_t pixel) const {
      const Entry* best = nullptr;
      int bestDistance = std::numeric_limits<int>::max();
      for (const Entry& entry : entries_) {
         const std::optional<int> distance = distanceBetween(entry.pixel, pixel);
         if (!distance) continue;
         if (!best || *distance < bestDistance || (*distance == bestDistance && better(entry, *best))) {
            best = &entry;
            bestDistance = *distance;
         }
      }
      if (!best) return std::nullopt;
      return best->picture;
   }

   // How far apart two pixels are, or nothing when too far to be one colour. Blending works on the
   // stored pixels, so a colour may land a step off its own.
   static std::optional<int> distanceBetween(uint16_t a, uint16_t b) {
      const Channels x = channels(a), y = channels(b);
      const int dr = std::abs(x.r - y.r), dg = std::abs(x.g - y.g), db = std::abs(x.b - y.b);
      if (dr > 1 || dg > 2 || db > 1) return std::nullopt;
      return dr * dr * 4 + dg * dg + db * db * 4;
   }

private:
   void add(ByteColor colour, Picture picture, bool blended, const std::byte* prototype) {
      Entry entry{pack(colour), picture, rankOf(picture.kind, blended)};
      if (prototype) {
         entry.hidden = at<bool>(prototype, layout.prototypeHidden);
         entry.order = reinterpret_cast<const MsvcString*>(prototype + layout.prototypeOrder)->view();
      }
      entries_.push_back(entry);
   }

   // Of two things drawn alike: the more likely kind, one a player can build, the first by order.
   static bool better(const Entry& a, const Entry& b) {
      if (a.rank != b.rank) return a.rank < b.rank;
      if (a.hidden != b.hidden) return !a.hidden;
      return a.order < b.order;
   }

   const std::byte* const* entityList_ = nullptr;
   const std::byte* const* tileList_ = nullptr;
   std::vector<Entry> entries_;
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
   if (tile && Table::distanceBetween(pack(at<ByteColor>(tile, layout.tilePrototypeMapColor)), *pixel) == 0)
      return Shown{Picture{Picture::Kind::Tile, tile}};
   g_table.rebuildIfStale();
   return Shown{g_table.lookup(*pixel)};
}

} // namespace fa::mappicture
