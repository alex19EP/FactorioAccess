#pragma once

// Where the cursor's keys take it (cursor.h), as positions alone: tiles, the tiles a view holds on
// the screen, the full map's cells. Pure, so the tests can check it without the game.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fa::cursormoves {

// MapPosition's fixed point: 256 to a tile (game::kMapPositionScale).
constexpr int32_t kScale = 256;

struct Position {
   int32_t x;
   int32_t y;
   bool operator==(const Position&) const = default;
};

struct Pixel {
   int32_t x;
   int32_t y;
};

inline int32_t tileOf(int32_t coordinate) { return coordinate >> 8; } // floors negative coordinates too
inline Position tileCentre(int32_t tileX, int32_t tileY) {
   return {tileX * kScale + kScale / 2, tileY * kScale + kScale / 2};
}
inline Position tileCentre(Position position) { return tileCentre(tileOf(position.x), tileOf(position.y)); }
inline bool sameTile(Position a, Position b) { return tileOf(a.x) == tileOf(b.x) && tileOf(a.y) == tileOf(b.y); }

// The tiles wholly on the screen, inclusive, as the view's centre and zoom place them.
struct Screen {
   int32_t left;
   int32_t top;
   int32_t right;
   int32_t bottom;

   bool holds(Position position) const {
      const int32_t x = tileOf(position.x), y = tileOf(position.y);
      return x >= left && x <= right && y >= top && y <= bottom;
   }
   Position clamp(Position position) const {
      return tileCentre(std::clamp(tileOf(position.x), left, right), std::clamp(tileOf(position.y), top, bottom));
   }
};

// A view of `size` pixels about `centre`, each pixel `tilesPerPixel` tiles across.
inline Screen screenOf(Position centre, Pixel size, double tilesPerPixel) {
   const double centreX = static_cast<double>(centre.x) / kScale;
   const double centreY = static_cast<double>(centre.y) / kScale;
   const double halfWidth = size.x * tilesPerPixel / 2;
   const double halfHeight = size.y * tilesPerPixel / 2;
   return {static_cast<int32_t>(std::ceil(centreX - halfWidth)), static_cast<int32_t>(std::ceil(centreY - halfHeight)),
           static_cast<int32_t>(std::floor(centreX + halfWidth)) - 1,
           static_cast<int32_t>(std::floor(centreY + halfHeight)) - 1};
}

// On the full map a movement key moves a cell: a square of tiles about a thirtieth of the screen
// across, rounded to a power of two, on a grid of its own size (the mod's map-cells.lua reads it so
// too).
inline int32_t cellSize(Pixel size, double tilesPerPixel) {
   const double tiles = std::floor(std::max(size.x, size.y) * tilesPerPixel + 0.5);
   const double exponent = std::floor(std::log2(tiles / 30.0) + 0.5);
   return exponent < 0 ? 1 : int32_t{1} << static_cast<int>(exponent);
}

// Where a movement key going `dx`, `dy` takes the cursor: the next tile's centre, or with `cell`
// (the full map's cell size, zero elsewhere) the centre of the cell beyond, on the grid of cells.
inline Position stepTarget(Position cursor, int32_t dx, int32_t dy, int32_t cell) {
   if (cell <= 0) return tileCentre(tileOf(cursor.x) + dx, tileOf(cursor.y) + dy);
   const auto beyond = [&](int32_t tile, int32_t step) {
      const int32_t next = tile + step * cell;
      const int32_t left = next >= 0 ? next / cell * cell : -((-next + cell - 1) / cell) * cell;
      return left + cell / 2;
   };
   return tileCentre(beyond(tileOf(cursor.x), dx), beyond(tileOf(cursor.y), dy));
}

// How many tiles Ctrl with a movement key moves: the building in hand's width or height along the
// way it goes, or a tile for a building of one tile or nothing in hand (width and height zero).
inline int32_t bySizeShift(int32_t dx, int32_t width, int32_t height) {
   if (width + height <= 2) return 1;
   return dx != 0 ? width : height;
}

// Where the cursor goes when the screen left it behind (walking, zooming in): back to the tile the
// view is centred on when anchored to the character, else the nearest tile on the screen.
inline Position pulledOnto(const Screen& screen, Position cursor, Position centre, bool anchored) {
   return anchored ? tileCentre(centre) : screen.clamp(cursor);
}

} // namespace fa::cursormoves
