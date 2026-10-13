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

#include "agui.h"
#include "graph/GraphBuilder.hpp"

namespace fa::screens
{

/// Declares the schedule's two stops, keyed `key` and `key` + "/interrupts".
void AddSchedule(graph::GraphBuilder& builder, const std::string& key, const agui::Widget* schedule);

} // namespace fa::screens
