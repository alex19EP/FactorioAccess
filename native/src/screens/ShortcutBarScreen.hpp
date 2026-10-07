#pragma once

// The game's shortcut bar beside the quickbar (ShortcutBarGui), the part of the HUD after the
// quickbar on Ctrl+Tab (see parts.h).
//
// Two stops. The shortcuts, a row per row of buttons as the bar lays them out, each named with its
// shortcut; one that stays on or off (the personal roboport, alt mode) says whether it is pressed.
// Then the button that opens the list of every shortcut, read instead as that list while it is
// open: a checkbox per shortcut, checked while it is on the bar.
//
// Every shortcut is the game's own button, clicked as the Gui clicks it, so Enter does what a click
// does in vanilla: it gives the planner or remote, undoes, toggles.
//
// The game has no window to close here, so Escape is ours: it closes the list of shortcuts, and
// otherwise goes back to what is open.

#include <string>

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class ShortcutBarScreen final : public nav::Screen
{
public:
    const char* Name() const override;
    const char* DiagName() const override { return "shortcut bar"; }
    // Back on the shortcut it was left on, after Ctrl+Tab went to the window and came back.
    bool RemembersCursor() const override { return true; }
    // Over the window it was opened from, as the quickbar is.
    int Layer() const override { return 1; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    const char* TakeSuggestedLanding() override;
    bool ClaimsEscape() const override { return true; }
    void OnEscape() override;
    void OnPop() override;
    std::string LeaveLine() const override;

private:
    const agui::Widget* _bar = nullptr;
    bool _listing = false;   // the list of shortcuts was open at the last build
    std::string _landing;    // a one-shot landing for the next render
    std::string _taken;      // the landing handed out, kept alive for the caller
};

} // namespace fa::screens
