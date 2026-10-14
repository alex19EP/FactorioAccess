#pragma once

// The pins panel at the right (PinsGui), the part of the HUD after the map view options on Ctrl+Tab
// (see parts.h). The game shows it in every view while the player has a pin.
//
// One stop, a row per pin in the player's order, each the game's own buttons: the pin's name as the
// game words it (its label, else what it points at, "{12, -40}" for a place), then edit and remove.
// Enter on the name clicks it as the mouse would, which opens remote view on the pin; Shift+Enter
// sends the train the player rides to it, the right click the selected spidertrons. Edit opens the
// pin's settings (label, always visible, preview distance), whose window's own screen reads them
// while they show; remove deletes the pin, and the cursor goes to the name of the pin taking its
// place. Ctrl+Shift+Up/Down moves the pin as dragging it does.
//
// The game has no window to close here, so Escape is ours: it goes back to what is open.

#include <functional>
#include <optional>
#include <string>

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class PinsScreen final : public nav::Screen
{
public:
    std::string Name() const override;
    const char* DiagName() const override { return "pins"; }
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
    // Where the cursor goes once the game has rebuilt the rows after a pin moved or went.
    struct Landing
    {
        std::string key;
        std::function<bool()> ready;
        int builds = 0;
    };

    const agui::Widget* _panel = nullptr;
    std::optional<Landing> _landing;
    std::string _landed;
};

} // namespace fa::screens
