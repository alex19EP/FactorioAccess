// How the map picture tells a chart pixel back from its colour (src/mapcolours.h): pixels packed as
// the chart packs them, blending a see-through colour, how near is one colour, and which of the
// things drawn alike the pixel is taken for.

#include <cstdint>
#include <optional>

#include "mapcolours.h"
#include "TestHarness.hpp"

using namespace fa::mappicture;

namespace
{

// Prototypes are told apart by address alone: members of one object, which the linker cannot fold.
struct Prototypes
{
    int Grass, Sand, Water, Belt, FastBelt, HiddenBelt, Tree;
} g_prototypes;
const int& kGrass = g_prototypes.Grass;
const int& kSand = g_prototypes.Sand;
const int& kWater = g_prototypes.Water;
const int& kBelt = g_prototypes.Belt;
const int& kFastBelt = g_prototypes.FastBelt;
const int& kHiddenBelt = g_prototypes.HiddenBelt;
const int& kTree = g_prototypes.Tree;

constexpr ByteColor kGrassColour{0x40, 0x80, 0x20, 0xff};
constexpr ByteColor kSandColour{0xc0, 0xa0, 0x60, 0xff};
constexpr ByteColor kWaterColour{0x20, 0x40, 0xc0, 0xff};
constexpr ByteColor kBeltColour{0xe0, 0xc0, 0x00, 0xff};

const void* PrototypeOf(std::optional<Picture> picture)
{
    return picture ? picture->prototype : nullptr;
}

// One step of a channel at a time, on the packed pixel.
constexpr uint16_t kRed = 1 << 11;
constexpr uint16_t kGreen = 1 << 5;
constexpr uint16_t kBlue = 1;

} // namespace

TEST(MapColours_PackKeepsTheTopBitsOfEachChannel)
{
    CHECK_EQ(0xffff, pack({0xff, 0xff, 0xff, 0xff}));
    CHECK_EQ(0, pack({0x07, 0x03, 0x07, 0xff}));
    CHECK_EQ(kRed | kGreen | kBlue, pack({0x08, 0x04, 0x08, 0xff}));
    // Read back and packed again, every pixel is itself.
    for (uint32_t pixel = 0; pixel <= 0xffff; ++pixel)
    {
        CHECK_EQ(pixel, pack(widen(static_cast<uint16_t>(pixel))));
    }
}

TEST(MapColours_BlendGoesByAlpha)
{
    const ByteColor under{0x10, 0x20, 0x30, 0xff};
    CHECK((ByteColor{0xc8, 0x64, 0x00, 0xff}) == blend({0xc8, 0x64, 0x00, 0xff}, under));
    CHECK(under == blend({0xc8, 0x64, 0x00, 0x00}, under));
    // Half over black: alpha 128 weighs 129 of 256.
    CHECK((ByteColor{100, 50, 0, 0xff}) == blend({200, 100, 0, 128}, {0, 0, 0, 0xff}));
}

TEST(MapColours_OneStepOffIsStillTheColour)
{
    const uint16_t pixel = pack(kSandColour);
    CHECK_EQ(std::optional<int>(0), distanceBetween(pixel, pixel));
    CHECK_EQ(std::optional<int>(4), distanceBetween(pixel, pixel + kRed));
    CHECK_EQ(std::optional<int>(4), distanceBetween(pixel, pixel - 2 * kGreen));
    CHECK_EQ(std::optional<int>(4), distanceBetween(pixel, pixel + kBlue));
    CHECK_EQ(std::optional<int>(9), distanceBetween(pixel, pixel + kRed + kGreen + kBlue));
    CHECK_FALSE(distanceBetween(pixel, pixel + 2 * kRed).has_value());
    CHECK_FALSE(distanceBetween(pixel, pixel + 3 * kGreen).has_value());
    CHECK_FALSE(distanceBetween(pixel, pixel - 2 * kBlue).has_value());
}

TEST(MapColours_PixelIsTheNearestColour)
{
    Colours colours;
    colours.addTile(kGrassColour, &kGrass, false, "a");
    colours.addTile(kSandColour, &kSand, false, "b");
    colours.addTile(kWaterColour, &kWater, false, "c");
    CHECK_EQ(static_cast<const void*>(&kSand), PrototypeOf(colours.lookup(pack(kSandColour))));
    CHECK_EQ(static_cast<const void*>(&kWater), PrototypeOf(colours.lookup(pack(kWaterColour) + kBlue)));
    CHECK_FALSE(colours.lookup(pack({0xff, 0x00, 0xff, 0xff})).has_value());

    // Two colours near enough to each other: the nearer. Two green steps apart, and "b" sorts last.
    Colours close;
    const ByteColor greener{0x40, 0x88, 0x20, 0xff};
    close.addTile(kGrassColour, &kGrass, false, "a");
    close.addTile(greener, &kSand, false, "b");
    CHECK_EQ(static_cast<const void*>(&kSand), PrototypeOf(close.lookup(pack(greener))));
    CHECK_EQ(static_cast<const void*>(&kSand), PrototypeOf(close.lookup(pack(greener) + kRed)));
    CHECK_EQ(static_cast<const void*>(&kGrass), PrototypeOf(close.lookup(pack(kGrassColour) - kBlue)));
}

TEST(MapColours_LikelierKindWinsATie)
{
    // A building in the ground's own colour: the ground.
    Colours tile;
    tile.addTile(kSandColour, &kSand, false, "a");
    tile.addKind(kSandColour, Picture::Kind::Building);
    CHECK(Picture::Kind::Tile == tile.lookup(pack(kSandColour))->kind);

    // Ghosts, then enemies, come before the ground.
    Colours ghost;
    ghost.addTile(kSandColour, &kSand, false, "a");
    ghost.addKind(kSandColour, Picture::Kind::Ghost);
    ghost.addKind(kSandColour, Picture::Kind::Enemy);
    CHECK(Picture::Kind::Ghost == ghost.lookup(pack(kSandColour))->kind);
    Colours enemy;
    enemy.addTile(kSandColour, &kSand, false, "a");
    enemy.addKind(kSandColour, Picture::Kind::Enemy);
    CHECK(Picture::Kind::Enemy == enemy.lookup(pack(kSandColour))->kind);

    // An entity's own colour before the default building colour.
    Colours entity;
    entity.addKind(kBeltColour, Picture::Kind::Building);
    entity.addEntity(kBeltColour, &kBelt, false, "a");
    CHECK(Picture::Kind::Entity == entity.lookup(pack(kBeltColour))->kind);
}

TEST(MapColours_AlikeEntitiesAreTheFirstBuildableByOrder)
{
    // Every belt tier is drawn in one colour: the first by order that a player can build.
    Colours colours;
    colours.addEntity(kBeltColour, &kHiddenBelt, true, "a");
    colours.addEntity(kBeltColour, &kFastBelt, false, "b-fast");
    colours.addEntity(kBeltColour, &kBelt, false, "a-plain");
    CHECK_EQ(static_cast<const void*>(&kBelt), PrototypeOf(colours.lookup(pack(kBeltColour))));
}

TEST(MapColours_SeeThroughColourIsFoundOverEachTile)
{
    // A tree's crown, half see-through, drawn over grass and over sand.
    Colours colours;
    colours.addTile(kGrassColour, &kGrass, false, "a");
    colours.addTile(kSandColour, &kSand, false, "b");
    const ByteColor crown{0x00, 0x60, 0x00, 0x80};
    colours.addEntity(crown, &kTree, false, "a");
    CHECK_EQ(static_cast<const void*>(&kTree),
        PrototypeOf(colours.lookup(pack(blend(crown, widen(pack(kGrassColour)))))));
    CHECK_EQ(static_cast<const void*>(&kTree),
        PrototypeOf(colours.lookup(pack(blend(crown, widen(pack(kSandColour)))))));
    // The bare ground stays the ground.
    CHECK_EQ(static_cast<const void*>(&kGrass), PrototypeOf(colours.lookup(pack(kGrassColour))));
}
