#pragma once

// Single player's Load game window (LoadMapGui). Stops:
//   saves   — the save list, read without touching the game; the window opens on it (SaveLists)
//   details — the selected save's map info: name, its action buttons, version, scenario, time
//             played, file size and the mods it was saved with
//   buttons — Back and Load
//   search  — the search field and the sort order buttons
//
// The cursor is not remembered: every opening starts on the saves.

#include "WindowScreen.hpp"

namespace fa::screens
{

class LoadGameScreen final : public WindowScreen
{
public:
    bool Handles(const agui::Widget* window) const override;
    bool RemembersCursor() const override { return false; }

protected:
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
