#pragma once

// A blueprint's picture as a canvas node (graph NodeVtable::OnMoveWithin): a cursor over the
// blueprint's tiles, read as the game draws each one (see preview.h). The arrows and WASD move it a
// tile, Shift with them to the next tile that reads differently, Home and End to the ends of the
// row; at the picture's edge the arrow moves the focus on. Enter is the game's left click on the
// tile (restore), ] its right click (remove), Shift+Enter its Shift click (the grid position).
// The read-coordinates key says the tile's place, counted from the top left corner.
//
// The game keeps no cursor in the picture, only the mouse; this one is the screen's own and holds
// the only view state of the screen that owns it.

#include <functional>
#include <string>

#include "graph/GraphBuilder.hpp"

namespace fa::agui
{
struct Widget;
}

namespace fa::screens
{

class PreviewCanvas
{
public:
    /// Declares the canvas node for `picture` under `key` into the builder's current stop.
    /// `gridPosition` reads the window's grid position as it shows it, said after Shift+Enter
    /// sets it; without one, Shift+Enter does nothing.
    void Add(graph::GraphBuilder& builder, const agui::Widget* picture, const std::string& key,
        std::function<std::string()> gridPosition = {});

private:
    const agui::Widget* _picture = nullptr;
    int _x = 0;
    int _y = 0;
    // Shift+Enter set the grid position: its next feedback is that position rather than the tile.
    bool _gridSet = false;
};

} // namespace fa::screens
