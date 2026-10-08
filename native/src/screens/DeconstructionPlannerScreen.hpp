#pragma once

// The deconstruction planner's window (DeconstructionItemGui), opened by a right click on the
// planner in a slot, beside the player's inventory.
//
// Four stops. The planner: its name, then the buttons for it in a row (rename, copy, export to a
// string, delete), and its description when it has one. The settings: trees and rocks only, the
// Entities and Tiles tabs, and the chosen tab's whitelist or blacklist switch and, for tiles, the
// tile mode. The chosen tab's filter slots, as the game lays them out, ten to a row: Enter opens
// the game's chooser, ] clears a slot. Then the player's inventory.
//
// Every control is the game's own, pressed as the Gui presses it, so what it does is vanilla.

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class DeconstructionPlannerScreen final : public EntityWindowScreen
{
protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
