#pragma once

// Single player's Load game window (LoadMapGui). Stops:
//   saves   — the save list, read without touching the game. Enter clicks a save, selecting it
//             (the details then show it); Enter on the selected save loads it, as a double-click
//             does. Backspace on the selected save asks to delete it (the details' delete button,
//             which confirms first)
//   details — the selected save's map info: name, its action buttons, version, scenario, time
//             played, file size and the mods it was saved with
//   buttons — Back and Load
//   search  — the search field and the sort order buttons

#include "WindowScreen.hpp"

namespace fa::screens
{

class LoadGameScreen final : public WindowScreen
{
public:
    bool Handles(const agui::Widget* window) const override;

protected:
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
