#pragma once

// The alerts window over a loaded game (GameView::alertsOverview): one category's alerts, opened by
// that category's alert button (the status part of the HUD, Ctrl+Tab). It stacks over whatever
// window is open, so it takes the navigator from that window's screen while it shows; E or Escape
// closes it, as in vanilla.
//
// One stop, said as the window's title and the category ("Alerts, attack"). Per surface (headed by
// its name when there are several) a line per group of alerts, as the game words it: "Turret is
// under attack (3)". Enter clicks the group as the mouse would, which opens remote view on it and
// closes the window. Right reaches the group's pin button, which pins it to the pins panel. The
// tooltip key reads the group's tooltip.

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class AlertsScreen final : public nav::Screen
{
public:
    const char* Name() const override { return ""; }
    const char* DiagName() const override { return "alerts"; }
    // Over the inventory or entity window it stacks on, as the game draws it.
    int Layer() const override { return 1; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    void OnCursorMoved(const graph::GraphNode& node) override;
    void OnPop() override;

private:
    const agui::Widget* _window = nullptr;
};

} // namespace fa::screens
