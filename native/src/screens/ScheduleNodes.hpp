#pragma once

// A schedule as the game shows it (ScheduleGui): a locomotive's, and later a space platform's.
//
// Two stops:
//   - the schedule: the note of a train group's shared schedule, then a row per station (its name,
//     with "current stop" on the one the train heads to, then go to, change and remove), each
//     followed by a row per wait condition, the game's own widgets left to right (and or, the
//     condition and its values, remove), and the button adding a condition; then the button
//     adding a station. Ctrl+Up and Ctrl+Down move between the stations;
//   - the interrupts: a row each (its name, go to, edit, remove), then the button adding one.
//
// Ctrl+Shift+Up and Ctrl+Shift+Down move a station, a condition or an interrupt one place, as
// dragging it does, and the cursor follows it once the game shows it in its new place.

#include <functional>
#include <string>
#include <vector>

#include "AguiNodes.hpp"
#include "agui.h"
#include "graph/GraphBuilder.hpp"

namespace fa::screens
{

/// The game's widgets as they are now, read afresh: lists the game rebuilds after a change.
using StationsNow = std::function<std::vector<agui::ScheduleStation>()>;
using ConditionsNow = std::function<std::vector<const agui::Widget*>()>;

/// Declares the schedule's two stops, keyed `key` and `key` + "/interrupts".
void AddSchedule(
    graph::GraphBuilder& builder, const std::string& key, const agui::Widget* schedule, const Lander& land);

/// Into the current stop: a row per station of `pane`, each followed by its wait conditions,
/// Ctrl+Up and Ctrl+Down moving between them, then `add`, the button adding a station.
void AddStations(graph::GraphBuilder& builder, const std::string& key, const StationsNow& now, const agui::Widget* pane,
    const agui::Widget* add, const Lander& land);

/// Into the current stop: a row per wait condition (ScheduleConditionGui) of `pane`, the game's own
/// widgets left to right, then `add`, the button adding a condition.
void AddConditions(graph::GraphBuilder& builder, const std::string& key, const ConditionsNow& now,
    const agui::Widget* pane, const agui::Widget* add, const Lander& land);

} // namespace fa::screens
