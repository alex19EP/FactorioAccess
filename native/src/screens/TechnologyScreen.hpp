#pragma once

// The technology window over a loaded game (GameView::technologyGui), opened by the game's own
// open-technology-gui control (T) or the research box. Like Factoriopedia it stacks over whatever
// window is open, so it takes the navigator from that window's screen while it shows.
//
// Stops, in this order:
//   - queue: the research queue, the research going on first, each technology with the button
//     that takes it out of the queue beside it;
//   - list: every technology, a grid the game's search (Ctrl+F) filters, the search field over it
//     while it is open;
//   - selected: the selected technology's name and status, then its details: cost, effects,
//     description and the Start research button;
//   - controls: Back and Forward through the selection history, close, and "Show only essential
//     technologies";
//   - graph: the selected technology's prerequisites and unlocks as the game draws them, in layers
//     top to bottom. Up goes to a prerequisite in the layers above, Down to a technology it unlocks
//     below, along the lines the game draws, landing on the one nearest across; Left and Right move
//     along the layer.
//
// A technology reads its name with its level, then its status as the game words the selected one's,
// its place in the queue and how much of it is researched. Its button is clicked as the mouse
// would: Enter selects it, which centres the graph on it; Shift+Enter starts its research or queues
// it. The tooltip key reads the game's tooltip. Whenever another technology becomes the selected
// one, the graph stop is entered on it.

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class TechnologyScreen final : public nav::Screen
{
public:
    const char* Name() const override { return ""; }
    const char* DiagName() const override { return "technology"; }
    // Over the inventory or entity window it stacks on, as the game draws it.
    int Layer() const override { return 1; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    const char* TakeSuggestedLanding() override;
    bool TypingIn(const graph::GraphNode& node) override;
    void OnCursorMoved(const graph::GraphNode& node) override;
    void OnPop() override;

private:
    const agui::Widget* _window = nullptr;
    bool _searching = false;        // the search field showed at the last build
    const char* _landing = nullptr; // where the next render lands, once
};

} // namespace fa::screens
