#pragma once

// The alerts configuration window (AlertsConfigGui), opened by the side menu's Alerts button in
// place of the inventory. It lists every alert type the game has, each with whether it is muted and
// whether it is disabled, and when it last happened.
//
// Stops, read in the window title's context:
//   - alerts: a row per alert type in the table's order. Its name with when it last happened
//     ("Entity destroyed, 5 min ago", "Train out of fuel, never"), then its Muted and Disabled
//     checkboxes, each named for the alert and its column so that Up and Down along a column say
//     whose each is. Enter ticks one as a click does. Only the alerts with a sound can be muted; the
//     game greys out the others' checkbox, which is said as "no sound".
//   - sort: the column headings' sort buttons, "sort by Name", with the order while the table is
//     sorted by that column.

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class AlertsConfigScreen final : public EntityWindowScreen
{
protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
