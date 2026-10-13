#pragma once

// The game's trains overview (TrainsGui), opened by its own control (open-trains-gui, O) or the
// side menu's Trains button.
//
// Stops:
//   - the overview: the surface chooser, then the Trains, Stations and Groups tabs;
//   - the selected tab's list: the schedules, the stations' names or the groups, choosing what the
//     cards show;
//   - the cards: on the Trains and Groups tabs a row per train, its state (Enter shows the train on
//     the map) then the button that opens its window; on the Stations tab a row per stop, the
//     game's own widgets left to right.

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class TrainsScreen final : public EntityWindowScreen
{
protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
