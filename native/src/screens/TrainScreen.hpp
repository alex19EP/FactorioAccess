#pragma once

// The game's window of a train (TrainGui), opened by a locomotive's open-gui control, or by a train
// card's minimap button in a train stop's window and the trains overview.
//
// Stops:
//   - the train: the window's controls in its name's context (the group, manual or automatic, drive
//     remotely), the title bar's buttons (rename, center the camera), then the Schedule and Cargo
//     tabs;
//   - while the Schedule tab shows, the schedule's two stops (ScheduleNodes.hpp): the stations with
//     their wait conditions, then the interrupts;
//   - while the Cargo tab shows, its contents.

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class TrainScreen final : public EntityWindowScreen
{
protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
