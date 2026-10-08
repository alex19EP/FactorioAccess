#pragma once

// The game's chooser of an item, entity, signal or upgrade (a SelectListGui<T>: FilterSelectGui<T>,
// IDWithQualityIDSelectListGui<T>), opened by a slot that takes a filter or an icon: an empty
// quickbar slot clicked with an empty hand, a filter slot of an inserter, a planner or a constant
// combinator, a blueprint's icon, and the like.
//
// One stop in the window's title ("Set filter"): its item group tabs, ended by the search button,
// above a grid of the selected group's choices, one row per subgroup line as the game lays them out.
// A choice reads its name only; Enter clicks it, which makes it the filter and closes the chooser.
// The game's search (Ctrl+F, or the search button) opens a field above them that filters the
// choices as it is typed in; the cursor lands on it. Escape is the game's own and closes the
// chooser unchanged.

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class FilterSelectScreen final : public nav::Screen
{
public:
    std::string Name() const override { return {}; }
    const char* DiagName() const override { return "filter chooser"; }
    // Over the window or quickbar it was opened from.
    int Layer() const override { return 1; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    const char* TakeSuggestedLanding() override;
    bool TypingIn(const graph::GraphNode& node) override;
    void OnCursorMoved(const graph::GraphNode& node) override;
    void OnPop() override;

private:
    const agui::Widget* _window = nullptr;
    bool _searching = false;    // the search field showed at the last build
    bool _landOnSearch = false; // it has just opened
};

} // namespace fa::screens
