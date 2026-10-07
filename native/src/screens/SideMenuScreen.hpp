#pragma once

// The game's side menu at the top right (SideMenu), the part of the HUD after the shortcut bar on
// Ctrl+Tab (see parts.h).
//
// One stop: its buttons as the game lays them out, rows of six. Each is the game's own button,
// named by its tooltip (Blueprint library, Production statistics, Bonuses, Factoriopedia, Trains,
// Achievements, Tips and tricks, Logistic networks, Players in multiplayer, Alerts, Mute), and
// clicked as the Gui clicks it. Every button but Mute opens a window, so pressing one leaves the
// side menu for that window, which its own screen then reads; Mute acts in place.
//
// The game has no window to close here, so Escape is ours: it goes back to what is open.

#include <string>

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class SideMenuScreen final : public nav::Screen
{
public:
    const char* Name() const override;
    const char* DiagName() const override { return "side menu"; }
    // Back on the button it was left on, after Ctrl+Tab went to the window and came back.
    bool RemembersCursor() const override { return true; }
    // Over the window it was opened from, as the quickbar is.
    int Layer() const override { return 1; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    bool ClaimsEscape() const override { return true; }
    void OnEscape() override;
    void OnPush() override;
    void OnPop() override;
    std::string LeaveLine() const override;

private:
    const agui::Widget* _menu = nullptr;
    // A button that opens a window was pressed; that window, not the map, is where the player goes.
    bool _opening = false;
};

} // namespace fa::screens
