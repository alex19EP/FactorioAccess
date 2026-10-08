#pragma once

// The upgrade planner's window (UpgradeItemGui), opened by a right click on the planner in a slot,
// beside the player's inventory.
//
// Three stops. The planner: its name, then the buttons for it in a row (rename, copy, export to a
// string, delete), and its description when it has one. The rules: the game lays them out four to
// a row under "From To" headers; here a row per rule, its From slot then its To slot, each named by
// its header. Every rule is a context of its number, so Up and Down say the rule they land on
// ("3, From, wooden chest") and Left and Right only the other side ("To, iron chest"). Enter opens
// the game's chooser, ] clears a slot. Then the player's inventory.
//
// Every control is the game's own, pressed as the Gui presses it, so what it does is vanilla.

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class UpgradePlannerScreen final : public EntityWindowScreen
{
protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
