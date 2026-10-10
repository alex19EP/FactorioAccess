// The rail kinds the tile reader says (src/railkinds.h), walked over rails laid out from the game's
// own rail data. Each 90 degree turn is laid piece by piece as a train drives it; a piece says its
// part of the turn only while all four pieces are built, and otherwise left or right of the way it
// heads.

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "railkinds.h"
#include "RailData.hpp"
#include "TestHarness.hpp"

using namespace fa::railkinds;
using fa::railtests::Extension;
using fa::railtests::kExtensions;
using fa::railtests::Offset;

namespace
{

const char* const kDirections[16] = {"north", "northnortheast", "northeast", "eastnortheast", "east", "eastsoutheast",
    "southeast", "southsoutheast", "south", "southsouthwest", "southwest", "westsouthwest", "west", "westnorthwest",
    "northwest", "northnorthwest"};

uint8_t Turned(uint8_t direction, int sixteenths) { return static_cast<uint8_t>((direction + sixteenths + 16) % 16); }

const Extension* FindExtension(RailType type, uint8_t placement, uint8_t end, uint8_t goal)
{
    for (const Extension& row : kExtensions)
    {
        if (row.Type == type && row.Placement == placement && row.End == end && row.Goal == goal)
        {
            return &row;
        }
    }
    return nullptr;
}

// A placement's two end directions, in the order the data lists them; the first is the front.
std::vector<uint8_t> EndsOf(RailType type, uint8_t placement)
{
    std::vector<uint8_t> ends;
    for (const Extension& row : kExtensions)
    {
        if (row.Type == type && row.Placement == placement && (ends.empty() || ends.back() != row.End))
        {
            ends.push_back(row.End);
        }
    }
    return ends;
}

Offset EndPosition(RailType type, uint8_t placement, uint8_t end)
{
    for (const Extension& row : kExtensions)
    {
        if (row.Type == type && row.Placement == placement && row.End == end)
        {
            return row.EndPosition;
        }
    }
    ::GraphTest::Fail("no such rail end", __FILE__, __LINE__);
}

struct PlacedRail
{
    RailType Type;
    uint8_t Placement;
    Offset Position;
};

// The rails built, joined as the game joins them: an end's left, straight and right neighbours are
// the rails its extensions name, where those are built.
class World
{
public:
    using Rail = const PlacedRail*;

    Rail Add(const PlacedRail& rail)
    {
        rails_.push_back(std::make_unique<PlacedRail>(rail));
        return rails_.back().get();
    }

    uint8_t placement(Rail rail) const { return rail->Placement; }

    uint8_t endDirection(Rail rail, int end) const
    {
        return EndsOf(rail->Type, rail->Placement).at(static_cast<size_t>(end));
    }

    Rail linked(Rail rail, int end, int turn) const
    {
        const uint8_t direction = endDirection(rail, end);
        const Extension* extension = FindExtension(rail->Type, rail->Placement, direction, Turned(direction, turn));
        if (!extension)
        {
            return nullptr;
        }
        const Offset at{rail->Position.X + extension->NextPosition.X, rail->Position.Y + extension->NextPosition.Y};
        for (const auto& other : rails_)
        {
            if (other->Type == extension->Next && other->Placement == extension->NextPlacement
                && other->Position.X == at.X && other->Position.Y == at.Y)
            {
                return other.get();
            }
        }
        return nullptr;
    }

private:
    std::vector<std::unique_ptr<PlacedRail>> rails_;
};

static_assert(RailGraph<World>);

// The four curves a train drives through from a straight rail at {1, 1}, leaving by `heading` and
// turning one sixteenth at each piece (-1 left, 1 right).
std::vector<PlacedRail> TurnPieces(uint8_t heading, int turn)
{
    PlacedRail at{RailType::Straight, 4, {1, 1}};
    uint8_t leaving = heading;
    std::vector<PlacedRail> pieces;
    for (int i = 0; i < 4; ++i)
    {
        const Extension* extension = FindExtension(at.Type, at.Placement, leaving, Turned(leaving, turn));
        CHECK(extension != nullptr);
        at = {extension->Next, extension->NextPlacement,
            {at.Position.X + extension->NextPosition.X, at.Position.Y + extension->NextPosition.Y}};
        leaving = extension->Goal;
        pieces.push_back(at);
    }
    return pieces;
}

// A lone curve, from its geometry: its even end faces back the way it heads, and its odd end
// turns one sixteenth left or right of that.
std::string LoneCurveKind(const PlacedRail& rail)
{
    const std::vector<uint8_t> ends = EndsOf(rail.Type, rail.Placement);
    CHECK_EQ(size_t{2}, ends.size());
    const uint8_t even = ends[0] % 2 == 0 ? ends[0] : ends[1];
    const uint8_t odd = ends[0] % 2 == 0 ? ends[1] : ends[0];
    const uint8_t heading = Turned(even, 8);
    CHECK(odd == Turned(heading, -1) || odd == Turned(heading, 1));
    return std::string(odd == Turned(heading, -1) ? "left" : "right") + "-of-" + kDirections[heading];
}

struct TurnCase
{
    uint8_t Heading;
    int Turn;
    const char* Name;
    bool GoesNorth;
};

constexpr TurnCase kTurnCases[] = {
    {4, -1, "east-to-north", true},
    {4, 1, "east-to-south", false},
    {12, 1, "west-to-north", true},
    {12, -1, "west-to-south", false},
};

} // namespace

TEST(RailData_ExtensionsMeetEndToEnd)
{
    for (const Extension& row : kExtensions)
    {
        const uint8_t entry = Turned(row.End, 8);
        const std::vector<uint8_t> nextEnds = EndsOf(row.Next, row.NextPlacement);
        CHECK(nextEnds.size() == 2);
        CHECK((nextEnds[0] == entry && nextEnds[1] == row.Goal) || (nextEnds[1] == entry && nextEnds[0] == row.Goal));
        const Offset back = EndPosition(row.Next, row.NextPlacement, entry);
        CHECK_EQ(row.EndPosition.X, row.NextPosition.X + back.X);
        CHECK_EQ(row.EndPosition.Y, row.NextPosition.Y + back.Y);
    }
}

TEST(RailKinds_TurnsCoverEveryCurvePlacement)
{
    std::set<std::pair<RailType, uint8_t>> seen;
    for (const TurnCase& turn : kTurnCases)
    {
        for (const PlacedRail& piece : TurnPieces(turn.Heading, turn.Turn))
        {
            seen.insert({piece.Type, piece.Placement});
        }
    }
    CHECK_EQ(size_t{16}, seen.size());
}

TEST(RailKinds_PieceSaysTheTurnOnlyWhenAllFourAreBuilt)
{
    for (const TurnCase& turn : kTurnCases)
    {
        const std::vector<PlacedRail> pieces = TurnPieces(turn.Heading, turn.Turn);
        const char* const southToNorth[4] = {"bottom", "lower-half", "upper-half", "top"};
        for (int i = 1; i < 4; ++i)
        {
            const int northward =
                pieces[static_cast<size_t>(i - 1)].Position.Y - pieces[static_cast<size_t>(i)].Position.Y;
            CHECK(turn.GoesNorth ? northward > 0 : northward < 0);
        }

        for (int built = 0; built < 16; ++built)
        {
            World world;
            std::vector<World::Rail> rails(4, nullptr);
            for (size_t i = 0; i < 4; ++i)
            {
                if (built & (1 << i))
                {
                    rails[i] = world.Add(pieces[i]);
                }
            }
            for (size_t i = 0; i < 4; ++i)
            {
                if (!rails[i])
                {
                    continue;
                }
                const std::string kind = kindOf(world, rails[i], pieces[i].Type, true);
                if (built == 15)
                {
                    const char* part = southToNorth[turn.GoesNorth ? i : 3 - i];
                    CHECK_EQ(std::string(part) + "-of-" + turn.Name + "-turn", kind);
                    CHECK_EQ(LoneCurveKind(pieces[i]), kindOf(world, rails[i], pieces[i].Type, false));
                }
                else
                {
                    CHECK_EQ(LoneCurveKind(pieces[i]), kind);
                }
            }
        }
    }
}

TEST(RailKinds_StraightAndHalfDiagonalGoByDirection)
{
    for (const Extension& row : kExtensions)
    {
        if (row.Type != RailType::Straight && row.Type != RailType::HalfDiagonal)
        {
            continue;
        }
        World world;
        const World::Rail rail = world.Add({row.Type, row.Placement, {0, 0}});
        const uint8_t back = world.endDirection(rail, kBack);
        CHECK_EQ(world.endDirection(rail, kFront) % 8, back % 8);
        const std::string expected = back % 8 == 0 ? "vertical" : back % 8 == 4 ? "horizontal" : kDirections[back % 8];
        CHECK_EQ(expected, kindOf(world, rail, row.Type, true));
    }
}
