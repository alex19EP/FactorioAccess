#pragma once

// The parts the game's train windows share: the lists of trains as cards (a TrainsWidget), in a
// train stop's window and in the trains overview.

#include <string>

#include "agui.h"
#include "graph/GraphBuilder.hpp"

namespace fa::screens
{

/// A TrainsWidget's cards, a row per train: the train's state, whose Enter shows the train on the
/// map, then the button that opens the train's window. Without trains, the subheader saying so.
/// Keys start with `prefix`.
void AddTrainCards(graph::GraphBuilder& builder, const std::string& prefix, const agui::Widget* trainsWidget);

/// A tabbed pane of train lists as one stop: the row of its tabs, then the selected tab's cards.
void AddTrainTabs(graph::GraphBuilder& builder, const std::string& key, const agui::Widget* tabs);

} // namespace fa::screens
