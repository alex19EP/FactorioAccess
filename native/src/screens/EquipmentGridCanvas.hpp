#pragma once

// An armor's or a vehicle's equipment grid as a canvas node (graph NodeVtable::OnMoveWithin): a
// cursor over its cells, which the game draws all in one widget (EquipmentGridGuiHolder). The
// arrows move it a cell; Shift with them skips to the next cell another piece covers, or to the
// edge; Home and End go to the ends of the row; at the grid's edge the arrow moves the focus on. A
// cell reads the piece covering it with its size ("battery 2, 1 by 2"), pieces of the same name
// numbered in reading order of their top left cells, or "empty"; with equipment in hand, whether
// that fits with its top left on the cell. The read-coordinates key says the cell's column and row.
//
// Enter, ], Shift+Enter and Ctrl+Enter are the game's clicks on the cell, so taking, moving and
// opening equipment are vanilla. With equipment in hand over an empty cell the click lands where
// the game puts the equipment's top left on the cell (it centres equipment on the mouse); where it
// does not fit, nothing is clicked and the cell says why.
//
// The game keeps no cursor in the grid, only the mouse. This one rests the grid's mouse on its cell
// (agui::hoverEquipmentCell), so the tooltip key, the pipette and the preview of the equipment in
// hand are the cell's. One grid shows at a time, so the cursor is the module's own.

#include <string>

#include "graph/GraphBuilder.hpp"

namespace fa::agui
{
struct Widget;
}

namespace fa::screens
{

/// Declares the canvas node for the grid `holder` draws under `key` into the builder's current stop.
void AddEquipmentGrid(graph::GraphBuilder& builder, const std::string& key, const agui::Widget* holder);

} // namespace fa::screens
