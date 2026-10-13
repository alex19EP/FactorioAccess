#pragma once

// The game's window of a train stop (TrainStopGui), opened by its own open-gui control.
//
// Stops:
//   - the stop: its name with how many stops share it and the rename button, the trains limit's
//     checkbox and number, the priority, and the circuit network button in the title bar;
//   - the trains: the tabs of the trains whose schedules have the stop and of the trains on the way
//     to it, then the selected tab's trains, a row each: the train's state (Enter shows the train on
//     the map), then the button that opens the train's window;
//   - the circuit network panel, while its button has it open.

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class TrainStopScreen final : public EntityWindowScreen
{
protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
