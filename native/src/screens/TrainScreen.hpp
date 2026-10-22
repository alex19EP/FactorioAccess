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
//
// The window opens docked in remote view, beside the map, which Ctrl+Tab goes to (see
// EntityWindowScreen): there the game's own Ctrl+click (add a temporary stop) and Shift+click (add a
// stop) act where the cursor is.

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class TrainScreen final : public EntityWindowScreen
{
public:
    bool IsActive() override;

protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;

private:
    /// Says a station or wait condition row the schedule gains: one added from the game's lists
    /// (focus stays on the add button, as the mouse does) or from the map.
    void WatchSchedule(const agui::Widget* window);

    /// The schedule's rows as last seen: each station's name and its wait conditions' text.
    struct ScheduleSeen
    {
        std::vector<std::string> stations;
        std::vector<std::vector<std::string>> conditions;
    };
    /// None while no train window shows.
    std::optional<ScheduleSeen> _seen;

    /// Says a label of the window that changes while it is open, as the train's status does.
    void WatchStatus(const agui::Widget* window);
    /// The window's labels as last seen; empty while no train window shows.
    std::vector<std::string> _labels;
};

} // namespace fa::screens
