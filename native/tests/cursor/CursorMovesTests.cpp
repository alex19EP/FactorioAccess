// Where the cursor's keys take it (src/cursormoves.h): a tile a step, the tiles on the screen, the
// full map's cells, Ctrl's move by the size of the building in hand, and where the cursor goes when
// the screen leaves it behind.

#include <cstdint>
#include <ostream>

#include "cursormoves.h"
#include "TestHarness.hpp"

using namespace fa::cursormoves;

namespace fa::cursormoves
{

std::ostream& operator<<(std::ostream& os, const Position& position)
{
    return os << "{" << position.x << ", " << position.y << "}";
}

} // namespace fa::cursormoves

namespace
{

Position Tile(int32_t x, int32_t y)
{
    return tileCentre(x, y);
}

// A 1920 by 1080 view at zoom 1: 32 pixels to a tile.
constexpr Pixel kFullHd{1920, 1080};
constexpr double kZoomOne = 1.0 / 32.0;

} // namespace

TEST(Cursor_TilesFloorNegativeCoordinates)
{
    CHECK_EQ(0, tileOf(0));
    CHECK_EQ(0, tileOf(255));
    CHECK_EQ(-1, tileOf(-1));
    CHECK_EQ(-1, tileOf(-256));
    CHECK_EQ(-2, tileOf(-257));
    CHECK_EQ((Position{-128, 384}), Tile(-1, 1));
    CHECK(sameTile(Position{-1, -1}, Position{-256, -200}));
    CHECK_FALSE(sameTile(Position{0, 0}, Position{-1, 0}));
}

TEST(Cursor_MovementKeysStepOneTile)
{
    const Position from = Tile(3, -2);
    CHECK_EQ(Tile(3, -3), stepTarget(from, 0, -1, 0)); // W
    CHECK_EQ(Tile(2, -2), stepTarget(from, -1, 0, 0)); // A
    CHECK_EQ(Tile(3, -1), stepTarget(from, 0, 1, 0));  // S
    CHECK_EQ(Tile(4, -2), stepTarget(from, 1, 0, 0));  // D
}

TEST(Cursor_StepGoesBackToTheTileCentre)
{
    // The cycle key leaves the cursor off its tile's centre; a step lands on the next one's.
    const Position offCentre{3 * kScale + 10, -2 * kScale + 250};
    CHECK_EQ(Tile(4, -2), stepTarget(offCentre, 1, 0, 0));
    CHECK_EQ(Tile(3, -3), stepTarget(offCentre, 0, -1, 0));
}

TEST(Cursor_ScreenHoldsTheTilesWhollyOnIt)
{
    // 60 tiles by 33.75 about the origin: the part tiles at the top and bottom are not held.
    const Screen screen = screenOf(Position{0, 0}, kFullHd, kZoomOne);
    CHECK_EQ(-30, screen.left);
    CHECK_EQ(29, screen.right);
    CHECK_EQ(-16, screen.top);
    CHECK_EQ(15, screen.bottom);
    CHECK(screen.holds(Tile(-30, -16)));
    CHECK(screen.holds(Tile(29, 15)));
    CHECK_FALSE(screen.holds(Tile(30, 0)));
    CHECK_FALSE(screen.holds(Tile(0, -17)));
}

TEST(Cursor_ScreenFollowsTheCentreAndZoom)
{
    // Zoomed out to 0.5 on {100.5, -20.5}: 120 tiles by 67.5.
    const Screen screen = screenOf(Tile(100, -21), kFullHd, 1.0 / 16.0);
    CHECK_EQ(41, screen.left);
    CHECK_EQ(159, screen.right);
    CHECK_EQ(-54, screen.top);
    CHECK_EQ(12, screen.bottom);
}

TEST(Cursor_StepAtTheScreenEdgeLeavesTheScreen)
{
    const Screen screen = screenOf(Position{0, 0}, kFullHd, kZoomOne);
    CHECK(screen.holds(stepTarget(Tile(28, 0), 1, 0, 0)));
    CHECK_FALSE(screen.holds(stepTarget(Tile(29, 0), 1, 0, 0)));
    CHECK_FALSE(screen.holds(stepTarget(Tile(0, -16), 0, -1, 0)));
}

TEST(Cursor_CellIsAThirtiethOfTheScreenToAPowerOfTwo)
{
    // 1920 pixels across at so many tiles a pixel.
    CHECK_EQ(1, cellSize(kFullHd, 20.0 / 1920));
    CHECK_EQ(1, cellSize(kFullHd, 30.0 / 1920));
    CHECK_EQ(4, cellSize(kFullHd, 100.0 / 1920));
    CHECK_EQ(8, cellSize(kFullHd, 240.0 / 1920));
    CHECK_EQ(32, cellSize(kFullHd, 960.0 / 1920));
    // The longer side counts, held upright too.
    CHECK_EQ(32, cellSize(Pixel{1080, 1920}, 960.0 / 1920));
}

TEST(Cursor_FullMapMovesToTheNextCellsCentre)
{
    // Cells of 8 tiles: 0 to 7 with centre 4, 8 to 15 with centre 12, -8 to -1 with centre -4.
    CHECK_EQ(Tile(12, 4), stepTarget(Tile(4, 4), 1, 0, 8));
    CHECK_EQ(Tile(-4, 4), stepTarget(Tile(4, 4), -1, 0, 8));
    CHECK_EQ(Tile(4, -4), stepTarget(Tile(4, 4), 0, -1, 8));
    CHECK_EQ(Tile(4, 12), stepTarget(Tile(4, 4), 0, 1, 8));
    // From anywhere in a cell, the next cell's centre on both axes.
    CHECK_EQ(Tile(12, 4), stepTarget(Tile(1, 7), 1, 0, 8));
    CHECK_EQ(Tile(-12, -4), stepTarget(Tile(-1, -8), -1, 0, 8));
    CHECK_EQ(Tile(-4, -12), stepTarget(Tile(-8, -1), 0, -1, 8));
    // Cells of one tile are tiles.
    CHECK_EQ(Tile(-6, 3), stepTarget(Tile(-5, 3), -1, 0, 1));
}

TEST(Cursor_ControlMovesByTheBuildingInHand)
{
    CHECK_EQ(1, bySizeShift(1, 0, 0)); // nothing in hand
    CHECK_EQ(1, bySizeShift(1, 1, 1)); // a one-tile building
    CHECK_EQ(3, bySizeShift(1, 3, 5)); // across: its width
    CHECK_EQ(3, bySizeShift(-1, 3, 5));
    CHECK_EQ(5, bySizeShift(0, 3, 5)); // up and down: its height
    CHECK_EQ(2, bySizeShift(0, 1, 2)); // a two-tile building
}

TEST(Cursor_ScreenLeavingTheCursorPullsItBack)
{
    // Zoomed in or walked away: the cursor stays on the nearest tile the screen holds.
    const Screen screen = screenOf(Position{0, 0}, kFullHd, kZoomOne);
    CHECK_EQ(Tile(29, 5), pulledOnto(screen, Tile(40, 5), Position{0, 0}, false));
    CHECK_EQ(Tile(-30, -16), pulledOnto(screen, Tile(-50, -40), Position{0, 0}, false));
    CHECK_EQ(Tile(3, 15), pulledOnto(screen, Position{3 * kScale + 7, 90 * kScale}, Position{0, 0}, false));
    // Anchored, it goes back to the character, on whose tile the view is centred.
    CHECK_EQ(Tile(0, 0), pulledOnto(screen, Tile(40, 5), Position{0, 0}, true));
    CHECK_EQ(Tile(-3, 2), pulledOnto(screen, Tile(40, 5), Position{-3 * kScale + 40, 2 * kScale + 200}, true));
}
