#pragma once

// The game's window for an entity over a loaded game, opened by its own open-gui control: a chest,
// a furnace, a mining drill, an assembler, an inserter, a boiler, a lab, and every other entity the
// game builds a GameGuiWithControllerInventory for. Also the recipe list an assembler without a
// recipe opens instead (AssemblingMachineSelectRecipeGui).
//
// Two stops. The entity's part, in its name's context, read by the generic walker: its status,
// progress bars, fuel, input, output and module slots, recipe, filters and settings. Then the
// player's inventory as the game lays it out, a row of the grid per table row.
//
// Every slot is the game's own button, clicked as the Gui clicks it, so taking, placing, splitting
// and moving stacks between the two inventories is vanilla, held Shift and Control included.

#include <string>

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class MachineScreen final : public nav::Screen
{
public:
    const char* Name() const override { return ""; }
    const char* DiagName() const override { return _class.c_str(); }
    bool RemembersCursor() const override { return true; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    bool TypingIn(const graph::GraphNode& node) override;
    void OnCursorMoved(const graph::GraphNode& node) override;
    void OnPop() override;

private:
    const agui::Widget* _window = nullptr;
    std::string _class;
};

} // namespace fa::screens
