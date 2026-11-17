#pragma once

// The game's map view options at the right in remote view (MapViewOptionsGui), the part of the HUD
// after the side menu on Ctrl+Tab (see parts.h). The character view has none, so there the part
// says nothing.
//
// One stop, as the game lays it out: the add tag and add ping buttons, then on the map the table of
// overlay toggles, each named by its tooltip (logistic network, electric network, turret range,
// pollution, station names, player names, tags, worker robots, rail signal states, recipe icons,
// pipelines) and said pressed while on. Each is the game's own button, clicked as the Gui clicks it.
// The toggles change only what this client's map draws; the mod reads them to say the same things
// in the map's cells (fa_native.map_overlays).
//
// While a character rides a space platform, a stop before them holds the panel the game shows above
// them (RemoteViewGui): the drop to a planet button, which opens the planet list
// (DropToPlanetSelectionGui, read as any window), and the button cancelling the drop.
//
// The game has no window to close here, so Escape is ours: it goes back to what is open.

#include <string>

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class MapViewOptionsScreen final : public nav::Screen
{
public:
    std::string Name() const override;
    const char* DiagName() const override { return "map view options"; }
    // Back on the toggle it was left on, after Ctrl+Tab went to the window and came back.
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
    const agui::Widget* _options = nullptr;
    const agui::Widget* _panel = nullptr;
    // The drop button was pressed; the planet list, not the map, is where the player goes.
    bool _opening = false;
};

} // namespace fa::screens
