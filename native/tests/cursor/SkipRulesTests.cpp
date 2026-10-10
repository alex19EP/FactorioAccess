// Where Shift with a movement key stops (src/skiprules.h), over a world of tiles made up here: the
// first tile that differs from where the skip started, robots and corpses passed over, water, an
// underground's other end, the audio ruler's lines, the fog of war's picture, and the screen's edge.

#include <cstdint>
#include <map>
#include <optional>
#include <ostream>
#include <utility>

#include "skiprules.h"
#include "TestHarness.hpp"

using fa::skip::find;
using fa::skip::Result;
using fa::skip::Ruler;

namespace fa::skip
{

std::ostream& operator<<(std::ostream& os, const Result& result)
{
    const char* kinds[] = {"Found", "Limit", "Edge"};
    return os << kinds[static_cast<int>(result.kind)] << " {" << result.x << ", " << result.y << "} " << result.tiles
              << " tiles";
}

bool operator==(const Result& a, const Result& b)
{
    return a.kind == b.kind && a.x == b.x && a.y == b.y && a.tiles == b.tiles;
}

} // namespace fa::skip

namespace
{

struct UndergroundEnd
{
    int32_t tileX;
    int32_t tileY;
    int tiles;
};

struct Thing
{
    int Kind;
    int Direction = 0;
    bool PassedOver = false; // a robot or a corpse
    std::optional<UndergroundEnd> Underground;
    uint8_t UndergroundDirection = 0;
};

struct TileContent
{
    const Thing* Entity = nullptr;
    bool Water = false;
    std::optional<int> Shown; // the fog of war's picture
};

class World
{
public:
    using Entity = const Thing*;

    void Put(int32_t x, int32_t y, const Thing& thing)
    {
        tiles_[{x, y}].Entity = &thing;
    }
    void Water(int32_t x, int32_t y)
    {
        tiles_[{x, y}].Water = true;
    }
    void Fog(int32_t x, int32_t y, int picture)
    {
        tiles_[{x, y}].Shown = picture;
    }

    Entity selectedAt(int32_t x, int32_t y) const
    {
        return At(x, y).Entity;
    }
    bool waterAt(int32_t x, int32_t y) const
    {
        return At(x, y).Water;
    }
    std::optional<int> shownAt(int32_t x, int32_t y) const
    {
        return At(x, y).Shown;
    }
    bool skipsOver(Entity entity) const
    {
        return entity->PassedOver;
    }
    std::pair<int, int> likenessOf(Entity entity) const
    {
        return {entity->Kind, entity->Direction};
    }
    std::optional<UndergroundEnd> undergroundEnd(Entity entity, uint8_t direction) const
    {
        if (entity->Underground && entity->UndergroundDirection == direction)
        {
            return entity->Underground;
        }
        return std::nullopt;
    }

private:
    TileContent At(int32_t x, int32_t y) const
    {
        const auto found = tiles_.find({x, y});
        return found == tiles_.end() ? TileContent{} : found->second;
    }

    std::map<std::pair<int32_t, int32_t>, TileContent> tiles_;
};

static_assert(fa::skip::SkipWorld<World>);

constexpr int kLimit = 100;
const auto kAnywhere = [](int32_t, int32_t) { return true; };

Result Found(int32_t x, int32_t y, int tiles)
{
    return {Result::Kind::Found, x, y, tiles};
}

Result SkipEast(const World& world, std::optional<Ruler> ruler = std::nullopt)
{
    return find(world, 0, 0, 1, 0, kLimit, kAnywhere, ruler);
}

constexpr int kBelt = 1;
constexpr int kChest = 2;
constexpr int kRobot = 3;
constexpr int kMachine = 4;
constexpr int kEast = 4;
constexpr int kNorth = 0;

} // namespace

TEST(Skip_GroundStopsAtTheFirstEntity)
{
    World world;
    const Thing chest{kChest};
    world.Put(5, 0, chest);
    CHECK_EQ(Found(5, 0, 5), SkipEast(world));
}

TEST(Skip_EveryDirectionGoesItsWay)
{
    World world;
    const Thing chest{kChest};
    world.Put(-3, 0, chest);
    world.Put(0, -4, chest);
    world.Put(0, 2, chest);
    CHECK_EQ(Found(-3, 0, 3), find(world, 0, 0, -1, 0, kLimit, kAnywhere, std::nullopt));
    CHECK_EQ(Found(0, -4, 4), find(world, 0, 0, 0, -1, kLimit, kAnywhere, std::nullopt));
    CHECK_EQ(Found(0, 2, 2), find(world, 0, 0, 0, 1, kLimit, kAnywhere, std::nullopt));
}

TEST(Skip_AlikeEntitiesArePassedTillOneFacesElsewhere)
{
    World world;
    const Thing east[4] = {{kBelt, kEast}, {kBelt, kEast}, {kBelt, kEast}, {kBelt, kEast}};
    const Thing north{kBelt, kNorth};
    for (int x = 0; x < 4; ++x)
    {
        world.Put(x, 0, east[x]);
    }
    world.Put(4, 0, north);
    CHECK_EQ(Found(4, 0, 4), SkipEast(world));
}

TEST(Skip_AnotherKindOfEntityStops)
{
    World world;
    const Thing belt{kBelt, kEast};
    const Thing beltToo{kBelt, kEast};
    const Thing chest{kChest, kEast};
    world.Put(0, 0, belt);
    world.Put(1, 0, beltToo);
    world.Put(2, 0, chest);
    CHECK_EQ(Found(2, 0, 2), SkipEast(world));
}

TEST(Skip_OneEntityOverManyTilesIsPassed)
{
    // A machine three tiles across, then the ground beyond it.
    World world;
    const Thing machine{kMachine};
    for (int x = 0; x < 3; ++x)
    {
        world.Put(x, 0, machine);
    }
    CHECK_EQ(Found(3, 0, 3), SkipEast(world));
}

TEST(Skip_GroundAfterAnEntityStops)
{
    World world;
    const Thing belts[2] = {{kBelt, kEast}, {kBelt, kEast}};
    world.Put(0, 0, belts[0]);
    world.Put(1, 0, belts[1]);
    CHECK_EQ(Found(2, 0, 2), SkipEast(world));
}

TEST(Skip_RobotsAndCorpsesArePassedOver)
{
    World world;
    const Thing belts[4] = {{kBelt, kEast}, {kBelt, kEast}, {kBelt, kEast}, {kBelt, kEast}};
    const Thing robot{kRobot, 0, true};
    const Thing chest{kChest};
    world.Put(0, 0, belts[0]);
    world.Put(1, 0, belts[1]);
    world.Put(2, 0, robot); // over a belt the mouse does not reach
    world.Put(3, 0, belts[3]);
    world.Put(4, 0, chest);
    CHECK_EQ(Found(4, 0, 4), SkipEast(world));

    // From under a robot the skip starts from the ground.
    World ground;
    ground.Put(0, 0, robot);
    ground.Put(1, 0, robot);
    ground.Put(6, 0, chest);
    CHECK_EQ(Found(6, 0, 6), SkipEast(ground));
}

TEST(Skip_WaterStopsGroundAndGroundStopsWater)
{
    World world;
    world.Water(3, 0);
    world.Water(4, 0);
    CHECK_EQ(Found(3, 0, 3), SkipEast(world));

    World lake;
    for (int x = 0; x < 5; ++x)
    {
        lake.Water(x, 0);
    }
    CHECK_EQ(Found(5, 0, 5), SkipEast(lake));
}

TEST(Skip_UndergroundJumpsToItsOtherEnd)
{
    World world;
    Thing entrance{kBelt, kEast};
    entrance.Underground = UndergroundEnd{6, 0, 6};
    entrance.UndergroundDirection = kEast;
    world.Put(0, 0, entrance);
    CHECK_EQ(Found(6, 0, 6), SkipEast(world));

    // Only the way the other end lies: going back it is an ordinary entity.
    CHECK_EQ(Found(-1, 0, 1), find(world, 0, 0, -1, 0, kLimit, kAnywhere, std::nullopt));

    // An end off the screen keeps the cursor where it is.
    const auto nearby = [](int32_t x, int32_t) { return x < 5; };
    CHECK_EQ((Result{Result::Kind::Edge, 0, 0, 0}), find(world, 0, 0, 1, 0, kLimit, nearby, std::nullopt));
}

TEST(Skip_RulerStopsWhereItsLinesAreCrossed)
{
    World world;
    // Coming into line with the ruler's column.
    CHECK_EQ(Found(4, 0, 4), SkipEast(world, Ruler{4, 10}));
    // Along the ruler's row, only at the ruler itself.
    CHECK_EQ(Found(7, 10, 7), find(world, 0, 10, 1, 0, kLimit, kAnywhere, Ruler{7, 10}));
    // Leaving its column.
    CHECK_EQ(Found(1, 0, 1), SkipEast(world, Ruler{0, 10}));
}

TEST(Skip_FogGoesByThePictureItShows)
{
    World world;
    for (int x = 0; x < 4; ++x)
    {
        world.Fog(x, 0, 1);
    }
    world.Fog(4, 0, 2);
    CHECK_EQ(Found(4, 0, 4), SkipEast(world));

    // Where the fog ends, and where it begins.
    World edge;
    edge.Fog(0, 0, 1);
    edge.Fog(1, 0, 1);
    CHECK_EQ(Found(2, 0, 2), SkipEast(edge));
    World into;
    into.Fog(3, 0, 1);
    CHECK_EQ(Found(3, 0, 3), SkipEast(into));
}

TEST(Skip_StopsAtTheEdgeOfTheScreen)
{
    World world;
    const auto onScreen = [](int32_t x, int32_t) { return x <= 3; };
    CHECK_EQ((Result{Result::Kind::Edge, 3, 0, 3}), find(world, 0, 0, 1, 0, kLimit, onScreen, std::nullopt));
    CHECK_EQ((Result{Result::Kind::Edge, 3, 0, 0}), find(world, 3, 0, 1, 0, kLimit, onScreen, std::nullopt));
}

TEST(Skip_GivesUpAtTheLimit)
{
    World world;
    CHECK_EQ((Result{Result::Kind::Limit, 10, 0, 10}), find(world, 0, 0, 1, 0, 10, kAnywhere, std::nullopt));
    World lake;
    for (int x = 0; x <= 20; ++x)
    {
        lake.Water(x, 0);
    }
    CHECK_EQ((Result{Result::Kind::Limit, 10, 0, 10}), find(lake, 0, 0, 1, 0, 10, kAnywhere, std::nullopt));
}
