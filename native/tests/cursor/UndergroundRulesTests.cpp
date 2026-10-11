// Which underground belt pairs with which (src/undergroundrules.h), over belts laid out here: the
// nearest exit ahead within reach, an entrance between blocking, belts across the line and of
// another kind passed over, and an exit looking back for its entrance.

#include <cstdint>
#include <map>
#include <utility>

#include "undergroundrules.h"
#include "TestHarness.hpp"

using fa::undergroundrules::partner;

namespace
{

constexpr uint8_t kNorth = 0;
constexpr uint8_t kEast = 4;
constexpr uint8_t kSouth = 8;
constexpr uint8_t kWest = 12;

struct Belt
{
    int Kind;
    uint8_t Flow;
};

class Row
{
public:
    void Put(int32_t x, int32_t y, const Belt& belt) { belts_[{x, y}] = &belt; }

    // The partner of the belt of `kind` on (x, y) flowing `flow`, within `reach`.
    const Belt* PartnerOf(int32_t x, int32_t y, int kind, uint8_t flow, int reach) const
    {
        auto sameKindAt = [&](int32_t atX, int32_t atY) -> const Belt*
        {
            const auto found = belts_.find({atX, atY});
            return found != belts_.end() && found->second->Kind == kind ? found->second : nullptr;
        };
        return partner<const Belt*>(x, y, flow, reach, sameKindAt, [](const Belt* belt) { return belt->Flow; });
    }

private:
    std::map<std::pair<int32_t, int32_t>, const Belt*> belts_;
};

constexpr int kYellow = 1;
constexpr int kRed = 2;
constexpr int kReach = 5;

} // namespace

TEST(Underground_EntrancePairsWithTheExitAhead)
{
    Row row;
    // An exit's items flow on the way it faces, which reads as the other way from its own tile.
    const Belt exit{kYellow, kWest};
    row.Put(4, 0, exit);
    CHECK(row.PartnerOf(0, 0, kYellow, kEast, kReach) == &exit);
}

TEST(Underground_ExitLooksBackForItsEntrance)
{
    Row row;
    const Belt entrance{kYellow, kEast};
    row.Put(0, 0, entrance);
    CHECK(row.PartnerOf(4, 0, kYellow, kWest, kReach) == &entrance);
}

TEST(Underground_TheNearestExitWins)
{
    Row row;
    const Belt nearer{kYellow, kSouth};
    const Belt farther{kYellow, kSouth};
    row.Put(0, -2, nearer);
    row.Put(0, -4, farther);
    CHECK(row.PartnerOf(0, 0, kYellow, kNorth, kReach) == &nearer);
}

TEST(Underground_AnEntranceBetweenBlocks)
{
    Row row;
    const Belt between{kYellow, kEast};
    const Belt exit{kYellow, kWest};
    row.Put(2, 0, between);
    row.Put(4, 0, exit);
    CHECK(row.PartnerOf(0, 0, kYellow, kEast, kReach) == nullptr);
    CHECK(row.PartnerOf(2, 0, kYellow, kEast, kReach) == &exit);
}

TEST(Underground_OtherKindsAndCrossingBeltsArePassedOver)
{
    Row row;
    const Belt red{kRed, kEast};
    const Belt crossing{kYellow, kNorth};
    const Belt exit{kYellow, kWest};
    row.Put(1, 0, red);
    row.Put(2, 0, crossing);
    row.Put(3, 0, exit);
    CHECK(row.PartnerOf(0, 0, kYellow, kEast, kReach) == &exit);
}

TEST(Underground_BeyondReachPairsWithNone)
{
    Row row;
    const Belt exit{kYellow, kWest};
    row.Put(kReach + 1, 0, exit);
    CHECK(row.PartnerOf(0, 0, kYellow, kEast, kReach) == nullptr);
    CHECK(row.PartnerOf(0, 0, kYellow, kEast, kReach + 1) == &exit);
}

TEST(Underground_AnExitBehindIsNoPartner)
{
    Row row;
    const Belt behind{kRed, kWest};
    row.Put(-2, 0, behind);
    CHECK(row.PartnerOf(0, 0, kRed, kEast, kReach) == nullptr);
}
