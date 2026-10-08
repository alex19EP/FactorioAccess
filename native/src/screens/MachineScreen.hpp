#pragma once

// The game's window for an entity over a loaded game, opened by its own open-gui control: a chest,
// a furnace, a mining drill, an assembler, an inserter, a boiler, a lab, and every other entity the
// game builds a GameGuiWithControllerInventory for, but the deconstruction planner's
// (DeconstructionPlannerScreen). Also the recipe list an assembler without a recipe opens instead
// (AssemblingMachineSelectRecipeGui).
//
// Two stops, or more. The entity's part, in its name's context, read by the generic walker: its status,
// progress bars, fuel, input, output and module slots, recipe, filters and settings. Then the
// player's inventory as the game lays it out, a row of the grid per table row. A window with
// network buttons in its title bar (a pump's, a boiler's) adds the panel a button opened. Then the
// views the mod attached to the window (a pump's: scripts/ui/fluid-views.lua).
//
// Every slot is the game's own button, clicked as the Gui clicks it, so taking, placing, splitting
// and moving stacks between the two inventories is vanilla, held Shift and Control included.

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class MachineScreen final : public EntityWindowScreen
{
protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
