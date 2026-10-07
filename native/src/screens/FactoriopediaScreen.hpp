#pragma once

// Factoriopedia over a loaded game (GameView::factoriopedia), opened from the side menu, its
// hotkey or a click on a recipe. It stacks over whatever window is open, the inventory or an
// entity's, so it takes the navigator from that window's screen while it shows.
//
// Three stops, in the window's order:
//   - header: the title bar's buttons, search, show unresearched and pin (both read as checkboxes),
//     back and forward through the browse history, and close;
//   - list: the item group tabs over a grid of the group's entries; Enter shows an entry's page;
//   - page: the entry's title, then its description a line each, then each section's heading and
//     its entries (made in, used in, ...), which Enter opens in turn. A description line with icons
//     the game makes clickable carries them beside it as links: Right reaches them, Enter clicks
//     one as the mouse would, the tooltip key reads its tooltip.
//
// Whenever another entry's page shows (chosen in the list, opened from a page, or the game's Back
// and Forward controls, Alt+Left and Alt+Right), the cursor lands on its title. Escape, E and the
// Factoriopedia hotkey close it as in vanilla.

#include <string>

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class FactoriopediaScreen final : public nav::Screen
{
public:
    const char* Name() const override { return ""; }
    const char* DiagName() const override { return "factoriopedia"; }
    // Over the inventory or entity window it stacks on, as the game draws it.
    int Layer() const override { return 1; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    const char* TakeSuggestedLanding() override;
    bool TypingIn(const graph::GraphNode& node) override;
    void OnCursorMoved(const graph::GraphNode& node) override;
    void OnPop() override;

private:
    const agui::Widget* _window = nullptr;
    std::string _title;           // the entry whose page showed at the last build
    bool _searching = false;      // the search field showed at the last build
    const char* _landing = nullptr; // where the next render lands, once
};

} // namespace fa::screens
