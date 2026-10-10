// The cycle key's stops in a tile (src/cyclestops.h), over selection boxes made up here: what the
// centre selects first, the rest in the order they take the mouse, the bare ground last, and the
// wrap from the last stop to the first.

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <vector>

#include "cyclestops.h"
#include "TestHarness.hpp"

using fa::cyclestops::Box;
using fa::cyclestops::kScale;
using fa::cyclestops::nextStop;
using Stop = fa::cyclestops::Stop<int>;

namespace
{

// An entity is its box's number in the list, from 1; 0 is none. A higher priority takes the mouse.
struct Thing
{
    Box Area;
    int Priority;
};

std::vector<Stop> StopsIn(const std::vector<Thing>& things)
{
    std::vector<Box> boxes;
    for (const Thing& thing : things)
    {
        boxes.push_back(thing.Area);
    }
    const auto selectAt = [&](int32_t x, int32_t y)
    {
        int best = 0;
        for (size_t i = 0; i < things.size(); ++i)
        {
            const Box& box = things[i].Area;
            const bool inside = x >= box.left && x < box.right && y >= box.top && y < box.bottom;
            if (inside && (best == 0 || things[i].Priority > things[static_cast<size_t>(best - 1)].Priority))
            {
                best = static_cast<int>(i + 1);
            }
        }
        return best;
    };
    const auto before = [&](int a, int b)
    { return things[static_cast<size_t>(a - 1)].Priority > things[static_cast<size_t>(b - 1)].Priority; };
    return fa::cyclestops::stopsIn<int>(0, 0, boxes, selectAt, before);
}

void CheckStop(const Stop& stop, int entity, int32_t x, int32_t y)
{
    CHECK_EQ(entity, stop.entity);
    CHECK_EQ(x, stop.x);
    CHECK_EQ(y, stop.y);
}

constexpr int32_t kCentre = kScale / 2;
constexpr Box kWholeTile{0, 0, kScale, kScale};

} // namespace

TEST(Cycle_BareTileIsOnlyGround)
{
    const std::vector<Stop> stops = StopsIn({});
    CHECK_EQ(size_t{1}, stops.size());
    CheckStop(stops[0], 0, kCentre, kCentre);
}

TEST(Cycle_BuildingOverTheTileHasNoGround)
{
    const std::vector<Stop> stops = StopsIn({{kWholeTile, 1}});
    CHECK_EQ(size_t{1}, stops.size());
    CheckStop(stops[0], 1, kCentre, kCentre);
}

TEST(Cycle_SmallThingIsFoundAtItsNearestPoint)
{
    // A belt over the tile, an inserter's box in its north-west corner: the centre selects the belt.
    const std::vector<Stop> stops = StopsIn({{kWholeTile, 1}, {{0, 0, 64, 64}, 5}});
    CHECK_EQ(size_t{2}, stops.size());
    CheckStop(stops[0], 1, kCentre, kCentre);
    CheckStop(stops[1], 2, 32, 32);
}

TEST(Cycle_CentreFirstThenWhatTakesTheMouse)
{
    // The centre's belt first though both others take the mouse from it, then the stronger.
    const std::vector<Stop> stops = StopsIn({{kWholeTile, 1}, {{0, 0, 64, 64}, 3}, {{192, 192, 256, 256}, 5}});
    CHECK_EQ(size_t{3}, stops.size());
    CheckStop(stops[0], 1, kCentre, kCentre);
    CheckStop(stops[1], 3, 224, 224);
    CheckStop(stops[2], 2, 32, 32);
}

TEST(Cycle_WithNothingAtTheCentreTheNearestComesFirstAndGroundLast)
{
    const std::vector<Stop> stops =
        StopsIn({{{100, 100, 120, 120}, 1}, {{0, 0, 32, 32}, 3}, {{224, 224, 256, 256}, 2}});
    CHECK_EQ(size_t{4}, stops.size());
    CheckStop(stops[0], 1, 110, 110);
    CheckStop(stops[1], 2, 16, 16);
    CheckStop(stops[2], 3, 240, 240);
    CheckStop(stops[3], 0, kCentre, kCentre);
}

TEST(Cycle_BoxFromTheNextTileCounts)
{
    // A tree's crown reaching a quarter into the tile from the west; a box only touching it does not.
    const std::vector<Stop> crown = StopsIn({{{-128, 0, 64, kScale}, 2}});
    CHECK_EQ(size_t{2}, crown.size());
    CheckStop(crown[0], 1, 32, kCentre);
    CheckStop(crown[1], 0, kCentre, kCentre);

    const std::vector<Stop> touching = StopsIn({{{-kScale, 0, 0, kScale}, 2}});
    CHECK_EQ(size_t{1}, touching.size());
    CheckStop(touching[0], 0, kCentre, kCentre);
}

TEST(Cycle_NextStopWrapsAround)
{
    const std::vector<Stop> stops{{4, 0, 0}, {7, 0, 0}, {0, 0, 0}};
    CHECK_EQ(size_t{1}, nextStop(stops, 4));
    CHECK_EQ(size_t{2}, nextStop(stops, 7));
    CHECK_EQ(size_t{0}, nextStop(stops, 0)); // from the ground, back to the first
    CHECK_EQ(size_t{0}, nextStop(stops, 9)); // what the mouse selects is none of them
}
