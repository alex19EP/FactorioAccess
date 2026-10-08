#pragma once

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

// What the picture shows on a tile: the entity the mouse would pick there, with its direction,
// whether it is removed, the items to be delivered to it and, while alt mode is on, its quality and
// the details the game draws on it (recipe, filters, signals); else the floor tile; else empty.
std::string describe(const agui::Widget* picture, int x, int y);

// Whether clicks change the blueprint: off in the previews of a book or the library.
bool editable(const agui::Widget* picture);

// The game's left click on a tile, which restores what is removed there, or its right click,
// which removes it. False when nothing is on the tile.
bool click(const agui::Widget* picture, int x, int y, bool right);

// Shift with the left click: the tile becomes the snapping grid's position, which the setup
// window's grid position fields then show.
void setGridPosition(const agui::Widget* picture, int x, int y);

} // namespace fa::preview
