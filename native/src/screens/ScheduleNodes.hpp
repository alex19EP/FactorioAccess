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

#include <string>
#include <vector>

#include "agui.h"
#include "graph/GraphBuilder.hpp"

namespace fa::screens
{

/// Declares the schedule's two stops, keyed `key` and `key` + "/interrupts".
void AddSchedule(graph::GraphBuilder& builder, const std::string& key, const agui::Widget* schedule);

/// Into the current stop: a row per station, each followed by its wait conditions, Ctrl+Up and
/// Ctrl+Down moving between them, then `add`, the button adding a station.
void AddStations(graph::GraphBuilder& builder, const std::string& key,
    const std::vector<agui::ScheduleStation>& stations, const agui::Widget* add);

/// Into the current stop: a row per wait condition (ScheduleConditionGui), the game's own widgets
/// left to right, then `add`, the button adding a condition.
void AddConditions(graph::GraphBuilder& builder, const std::string& key,
    const std::vector<const agui::Widget*>& conditions, const agui::Widget* add);

} // namespace fa::screens
