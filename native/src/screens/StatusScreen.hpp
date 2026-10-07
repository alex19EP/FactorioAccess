#pragma once

// The HUD's status, the part after the side menu on Ctrl+Tab (see parts.h): what the game shows
// around the edges of the map about how things stand.
//
// A stop each, for those the game shows:
//   - research: the research box at the top right, the technology and its progress; Enter opens the
//     technology tree;
//   - alerts: a button per alert category that has alerts (attack, construction, logistics, ...)
//     with the count of its most important alert; Enter opens the game's list of them, and the
//     tooltip key reads what the game's tooltip lists;
//   - goal: the scenario's goal at the top left;
//   - bars: health and shield, the vehicle's while driving, and mining while it goes on.
//
// The research box and the alert buttons open windows, so pressing one leaves the status for that
// window, which its own screen then reads. Escape is ours: it goes back to what is open.

#include <string>
#include <unordered_map>

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class StatusScreen final : public nav::Screen
{
public:
    const char* Name() const override;
    const char* DiagName() const override { return "status"; }
    // Back where it was left, after Ctrl+Tab went to the window and came back.
    bool RemembersCursor() const override { return true; }
    // Over the window it was opened from, as the quickbar is.
    int Layer() const override { return 1; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    bool ClaimsEscape() const override { return true; }
    void OnEscape() override;
    void OnPush() override;
    std::string LeaveLine() const override;

private:
    // Presses a button that opens a window, and leaves the status for it.
    void Open(const agui::Widget* button);
    // An alert button's count, which reads 0 while the button blinks: the last count seen otherwise.
    double AlertCount(const agui::Widget* button);

    bool _opening = false;
    std::unordered_map<const agui::Widget*, double> _alertCounts;
};

} // namespace fa::screens
