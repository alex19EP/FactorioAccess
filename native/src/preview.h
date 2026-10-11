#pragma once

#include "skiprules.h"

#include <optional>
#include <string>

namespace fa::agui {
struct Widget;
}

// A blueprint's picture (BlueprintWidget), in its setup window or in the preview of a book or the
// library: read a tile at a time as the game draws it, and clicked at a tile as the mouse would
// click there. The OS mouse never moves. Main thread, inside the Gui's logic.
namespace fa::preview {

// The tiles the blueprint covers, in the picture's map coordinates: left and top inclusive, right
// and bottom exclusive.
struct Box {
   int left = 0;
   int top = 0;
   int right = 0;
   int bottom = 0;
};
std::optional<Box> extent(const agui::Widget* picture);

// What the picture shows on a tile: the entity the mouse would pick there, read as the tile reader
// reads one in the world but for what only the world has (facts.h's describeDesign): its facing,
// a belt's shape, an underground belt's partner, what an inserter reaches and so on; whether it is
// removed, what is drawn on it at all times (a combinator's operation), while alt mode is on its
// quality and icons as the picture draws them, the items to be delivered to it, and the floor under
// it. With no entity there, the floor tile; else empty.
std::string describe(const agui::Widget* picture, int x, int y);

// Shift with an arrow, as the map cursor's skip goes (skiprules.h): from the tile `x`, `y` a tile
// at a time in the direction `dx`, `dy`, to the first tile that differs (another entity, or the
// same kind facing elsewhere, a belt fed otherwise, another floor), or from an underground belt to
// its partner. Edge when it meets the box's edge first.
skip::Result skipFrom(const agui::Widget* picture, int x, int y, int dx, int dy);

// Whether a step from the tile `fromX`, `fromY` onto `toX`, `toY` goes from an entity bigger than a
// tile onto another of its kind (facts::nextOfKind).
bool nextOfKind(const agui::Widget* picture, int fromX, int fromY, int toX, int toY);

// Whether clicks change the blueprint: off in the previews of a book or the library.
bool editable(const agui::Widget* picture);

// The game's left click on a tile, which restores what is removed there, or its right click,
// which removes it. False when nothing is on the tile.
bool click(const agui::Widget* picture, int x, int y, bool right);

// Shift with the left click: the tile becomes the snapping grid's position, which the setup
// window's grid position fields then show.
void setGridPosition(const agui::Widget* picture, int x, int y);

} // namespace fa::preview
