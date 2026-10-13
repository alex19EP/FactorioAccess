#pragma once

// The game's window editing an interrupt of a schedule (EditInterruptGui), opened by an interrupt's
// edit button or by adding one; it floats beside the train's window.
//
// Stops:
//   - the interrupt: its name and settings, in the window's title context;
//   - the conditions that trigger it: a row per condition, the game's own widgets left to right (and
//     or, the condition and its values, remove), then the button adding one;
//   - its targets: a row per station with its wait conditions under it, as a schedule's
//     (ScheduleNodes.hpp), then the button adding one.

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class InterruptScreen final : public EntityWindowScreen
{
protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
