#pragma once

// Single player's New game window (NewGameGui). Stops:
//   scenarios  — the scenarios, under their group headings ("Main game:", "Scenarios:"), read
//                without touching the game. Enter clicks one, selecting it (the details then show
//                it); Enter on the selected one continues, as a double-click does
//   levels     — a campaign's levels, the same way; only for campaigns
//   difficulty — the difficulty choice; only when the scenario has one
//   details    — the selected scenario's name, replay checkbox and delete button, then its
//                description
//   buttons    — Back and Continue
//   search     — the search field

#include "WindowScreen.hpp"

namespace fa::screens
{

class NewGameScreen final : public WindowScreen
{
public:
    bool Handles(const agui::Widget* window) const override;

protected:
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
