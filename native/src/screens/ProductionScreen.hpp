#pragma once

// The game's production statistics window (ProductionGui), opened by its own control
// (production-statistics, P) or the side menu's button. Both stops are read in the window title's
// context ("Nauvis production").
//
// Stops:
//   - controls: the tabs (Items, Fluids, Buildings, Pollution, Kills), then the time span buttons
//     (5s to 1000h, and All), then the search button with its field while the field is open;
//   - lists: the two columns of the tab shown, side by side as the game draws them (Production and
//     Consumption, Built and Deconstructed, Kills and Losses). Each column is its caption's context:
//     the controls the game shows at its top (global statistics, quality, reset filters), then a
//     row per entry in the game's order, the most first: the game draws them two wide, down the
//     first half and then the second, and they read as one list. Up and Down move along a column,
//     Left and Right to the same place in the other.
//
// A row reads the entry's name, its rate ("12.3k/m") and the total drawn on its icon ("45.3k"):
// the bar beside it only compares the row to the top one, which the order already tells. The
// tooltip key reads the exact rate, then the entry's own tooltip. The graphs over the columns are
// left out (todo.md), and with them the row clicks that filter what they draw.
//
// The window opens on the first row of the left column.

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class ProductionScreen final : public EntityWindowScreen
{
public:
    bool RemembersCursor() const override { return false; }
    void OnPop() override;

protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;

private:
    bool _searching = false; // the search field showed at the last build
};

} // namespace fa::screens
