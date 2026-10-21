#pragma once

// The logistics panel (ItemLogisticGui) the game puts in the window of whatever has logistic
// requests or filters: the character's Logistics part, a requester, buffer or storage chest, a
// roboport, a vehicle, a constant combinator, a space platform hub, a cargo landing pad.
//
// Into the builder's current stop:
//   - the point's options (Personal logistics, Trash unrequested, Request from buffer chests),
//     read as any window part is;
//   - each section as a region (Ctrl+Up and Ctrl+Down move between them): a row of its header,
//     the group checkbox (whether the section is active, captioned with its group's name), Change
//     logistic group and Remove, or the label a section the player cannot edit shows instead; then
//     its slots in the group's context, a row of the grid per row of the game's table;
//   - Add section.
//
// Ctrl+Shift+Up and Ctrl+Shift+Down on a section's header move the section one place, as dragging
// it does, and the cursor follows it there when the screen lands (BuildLander). A request slot
// reads its minimum and maximum ("iron plate 200 to 400"); digits typed on it set its minimum.

#include <string>

#include "agui.h"
#include "graph/GraphBuilder.hpp"

namespace fa::screens
{

/// Declares the panel `panel` (an ItemLogisticGui) into the builder's current stop, keys starting
/// with `key`.
void AddLogistics(graph::GraphBuilder& builder, const std::string& key, const agui::Widget* panel);

} // namespace fa::screens
