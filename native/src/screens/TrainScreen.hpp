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
// The window opens docked in remote view, beside the map. Ctrl+Tab goes to the map (parts::Part::Map),
// where this screen steps aside: FA's world keys move the cursor on the map, and the game's own
// Ctrl+click (add a temporary stop) and Shift+click (add a stop) act where it is.

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class TrainScreen final : public EntityWindowScreen
{
public:
    bool IsActive() override;
    std::string LeaveLine() const override;

protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;

private:
    /// On the map, says a station row the schedule gains, as the window beside the map shows it.
    void WatchSchedule(const agui::Widget* window);

    /// The schedule's rows the last frame on the map; none off it.
    std::optional<std::vector<std::string>> _mapStations;
};

} // namespace fa::screens
