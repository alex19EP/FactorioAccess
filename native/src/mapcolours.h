#pragma once

// How the map picture (mappicture.h) tells a chart pixel back from its colour: every colour the
// chart draws with, packed as the chart packs it, and the nearest of them to a pixel. Pure, so the
// tests can match colours of their own.

#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

namespace fa::mappicture {

struct Picture {
   enum class Kind { Tile, Entity, Building, Enemy, Ghost, TileGhost };
   Kind kind;
   const void* prototype; // the tile or entity prototype, for Tile and Entity
   bool operator==(const Picture&) const = default;
};

// ByteColor: r, g, b, a.
using ByteColor = std::array<uint8_t, 4>;

// A pixel's channels: 5 bits of red, 6 of green, 5 of blue.
struct Channels {
   int r;
   int g;
   int b;
};

inline uint16_t pack(ByteColor colour) {
   return static_cast<uint16_t>((colour[0] & 0xf8) << 8 | (colour[1] & 0xfc) << 3 | colour[2] >> 3);
}

inline Channels channels(uint16_t pixel) { return {pixel >> 11, pixel >> 5 & 0x3f, pixel & 0x1f}; }

// The pixel read back as eight bits a channel, as blending over it reads it.
inline ByteColor widen(uint16_t pixel) {
   const Channels c = channels(pixel);
   return {static_cast<uint8_t>(c.r << 3 | c.r >> 2), static_cast<uint8_t>(c.g << 2 | c.g >> 4),
           static_cast<uint8_t>(c.b << 3 | c.b >> 2), 0xff};
}

// ChartingInterface::drawRectangle: `colour` over `under` by its alpha.
inline ByteColor blend(ByteColor colour, ByteColor under) {
   const unsigned weight = colour[3] + (colour[3] >> 7), rest = 0x100 - weight;
   ByteColor out{};
   for (int i = 0; i < 3; ++i) out[i] = static_cast<uint8_t>((colour[i] * weight + under[i] * rest) >> 8);
   out[3] = 0xff;
   return out;
}

// How far apart two pixels are, or nothing when too far to be one colour. Blending works on the
// stored pixels, so a colour may land a step off its own.
inline std::optional<int> distanceBetween(uint16_t a, uint16_t b) {
   const Channels x = channels(a), y = channels(b);
   const int dr = std::abs(x.r - y.r), dg = std::abs(x.g - y.g), db = std::abs(x.b - y.b);
   if (dr > 1 || dg > 2 || db > 1) return std::nullopt;
   return dr * dr * 4 + dg * dg + db * db * 4;
}

// The colours the chart draws with, and what draws each. Tiles go in before entities, which a
// see-through colour (a tree's crown) is blended over.
class Colours {
public:
   void clear() {
      entries_.clear();
      tiles_.clear();
   }
   bool empty() const { return entries_.empty(); }

   void addTile(ByteColor colour, const void* prototype, bool hidden, std::string_view order) {
      tiles_.push_back(colour);
      add(colour, {Picture::Kind::Tile, prototype}, false, hidden, order);
   }

   // The colours of no one prototype: the default building colour, enemies, ghosts.
   void addKind(ByteColor colour, Picture::Kind kind) { add(colour, {kind, nullptr}, false, false, {}); }

   // An entity drawn in `colour` (Chart::drawEntity), see-through over each tile when not opaque.
   void addEntity(ByteColor colour, const void* prototype, bool hidden, std::string_view order) {
      const Picture picture{Picture::Kind::Entity, prototype};
      if (colour[3] == 0xff) {
         add(colour, picture, false, hidden, order);
         return;
      }
      for (const ByteColor under : tiles_) add(blend(colour, widen(pack(under))), picture, true, hidden, order);
   }

   // What the nearest colour to `pixel` pictures, nothing when none is near enough.
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

private:
   struct Entry {
      uint16_t pixel;
      Picture picture;
      int rank;
      bool hidden;
      std::string_view order;
   };

   // On a tie the lower rank wins: the ground is more often what a pixel shows than a building of
   // the same colour, and a translucent thing over a tile least often.
   static int rankOf(Picture::Kind kind, bool blended) {
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

   void add(ByteColor colour, Picture picture, bool blended, bool hidden, std::string_view order) {
      entries_.push_back({pack(colour), picture, rankOf(picture.kind, blended), hidden, order});
   }

   // Of two things drawn alike: the more likely kind, one a player can build, the first by order.
   static bool better(const Entry& a, const Entry& b) {
      if (a.rank != b.rank) return a.rank < b.rank;
      if (a.hidden != b.hidden) return !a.hidden;
      return a.order < b.order;
   }

   std::vector<Entry> entries_;
   std::vector<ByteColor> tiles_;
};

} // namespace fa::mappicture
