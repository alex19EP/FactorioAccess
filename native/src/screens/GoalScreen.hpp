#pragma once

// The scenario's goal window at the top left, the first part of the HUD on Ctrl+Tab (see parts.h),
// there only while the scenario shows a goal: the tutorials', the campaigns' and other scenarios'.
//
// One stop: the goal, then what the scenario adds under it in the window: the tutorial's hints, its
// drive progress bar, its table of items still to pack.
//
// The game has no window to close here, so Escape is ours: it goes back to what is open.

#include <string>

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class GoalScreen final : public nav::Screen
{
public:
    std::string Name() const override;
    const char* DiagName() const override { return "goal"; }
    bool RemembersCursor() const override { return true; }
    // Over the window it was opened from, as the quickbar is.
    int Layer() const override { return 1; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    bool ClaimsEscape() const override { return true; }
    void OnEscape() override;
    void OnPop() override;
    std::string LeaveLine() const override;

private:
    const agui::Widget* _goal = nullptr;
};

} // namespace fa::screens
