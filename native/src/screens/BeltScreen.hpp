#pragma once

// The game's window for a transport belt (TransportBeltGui), opened by its own open-gui control.
//
// The game shows little: the belt's name and status, and in the title bar the circuit and logistic
// network buttons, each opening a panel beside the window (the circuit one: enable or disable by a
// condition, and read the belt's contents by pulse or hold). What the belt carries the game shows
// only on the map, so the mod sends its own views of that (scripts/belt-analyzer.lua).
//
// Stops: the window, the open network panel, then the mod's views: this belt's slots, and what the
// whole belt, the belts feeding it and the belts it feeds carry on each lane.

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class BeltScreen final : public EntityWindowScreen
{
protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
