#pragma once

// The parts the Load game and Save game windows share: their list of saves and its search.

#include "agui.h"
#include "graph/GraphBuilder.hpp"

namespace fa::screens
{

/// The saves of a PackageListGui as the "saves" stop, read without touching the game; focus starts
/// on the selected one. Enter clicks a save, selecting it; Enter on the selected save presses the
/// window's confirm (Load, Save), as a double-click does. Backspace on the selected save presses
/// `remove`, the details' delete button, which asks first.
void AddSaves(graph::GraphBuilder& builder, const agui::Widget* window, const agui::Widget* list,
    const agui::Widget* remove);

/// The search field and the sort order buttons as the "search" stop.
void AddSaveSearch(graph::GraphBuilder& builder, const agui::Widget* window);

} // namespace fa::screens
