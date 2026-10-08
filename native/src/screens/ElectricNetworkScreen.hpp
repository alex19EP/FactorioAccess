#pragma once

// The game's electric network window (ElectricNetworkGuiWindow): an electric pole's, opened by its
// own open-gui control, which shows the pole's network, and the like window for every network of a
// surface.
//
// The window: the bars of how well the network meets demand, how much of what it could produce it
// produces, and how charged its accumulators are; the buttons choosing the span of time all of it
// is measured over; then the columns of what consumes, produces and stores the energy, each a table
// of a row per building with how many there are and their power. The graphs above the tables are
// left out (sonifying them is a project of its own, see todo.md).
//
// Stops: the bars with the time span buttons, production, consumption, accumulators while the
// game shows them, then for a pole the mod's views (scripts/ui/pole-views.lua): its wires, and
// what its supply area powers.

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class ElectricNetworkScreen final : public EntityWindowScreen
{
protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
