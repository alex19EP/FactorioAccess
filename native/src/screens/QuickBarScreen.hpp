#pragma once

// The game's quickbar along the bottom of the screen (QuickBarGui), a part of the HUD that Ctrl+Tab
// moves to from the world or from any window (see parts.h), and back again.
//
// Two stops. The bars on screen, the one the quickbar keys (1 to 0) use first, each a row of its
// slots read in the context of the page it shows. Then the button of each bar that shows its
// page's number; pressing it opens the game's page picker, read instead as a line per page: the
// page's button, which shows that page on the bar, then its slots.
//
// Every slot is the game's own button, clicked as the Gui clicks it, so what a click does is
// vanilla. Enter is the left button: with an item in hand it puts that item on the slot, with an
// empty hand it takes the slot's item, and on an empty slot it opens the game's item chooser.
// Backslash is the middle button, which clears a slot. Backspace is the right button.
//
// The game has no window to close here, so Escape is ours: it closes the page picker, and
// otherwise goes back to what is open.

#include <string>

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class QuickBarScreen final : public nav::Screen
{
public:
    const char* Name() const override;
    const char* DiagName() const override { return "quickbar"; }
    // Back on the slot it was left on, after Ctrl+Tab went to the window and came back.
    bool RemembersCursor() const override { return true; }
    // Over the window it was opened from, as the dropdowns and choosers that open from it are.
    int Layer() const override { return 1; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    const char* TakeSuggestedLanding() override;
    bool ClaimsEscape() const override { return true; }
    void OnEscape() override;
    void OnPop() override;
    std::string LeaveLine() const override;

    // Speaks the page the first bar shows when the game changes it while the quickbar is not in use
    // (the game's Shift+1 to 0), with the page's first slot. Once per frame, on the Gui's logic.
    static void WatchPage();

private:
    const agui::Widget* _bar = nullptr;
    int _picking = -1;      // the bar the page picker chose for at the last build, or -1
    bool _cancelled = false; // Escape closed the picker
    std::string _landing;   // a one-shot landing for the next render
    std::string _taken;     // the landing handed out, kept alive for the caller
};

} // namespace fa::screens
