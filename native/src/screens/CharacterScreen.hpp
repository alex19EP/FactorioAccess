#pragma once

// The game's character screen over a loaded game (ControllerGui, opened by the game's own
// open-character-gui control, E): the main inventory as a grid of slots, and the crafting list as
// its item group tabs above a grid of recipes, one row per subgroup line as the game lays them out.
//
// Every slot and recipe is the game's own button and is clicked as the Gui clicks it, so what a
// click does is vanilla: [ or Enter is the left button, ] or Backspace the right one, and a held
// Shift or Control counts because the game reads them from the keyboard. On a slot that picks up,
// places, splits and transfers stacks; on a recipe it crafts one, five or all.
//
// The game's search (Ctrl+F, or the search button that ends the row of tabs) opens a field above
// them that filters the recipes as it is typed in; the cursor lands on it.
//
// Slots read live, so a click's result is spoken once the game has applied it. Y reads the
// game's own tooltip, and \ is the middle button, which sets a slot's filter.

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class CharacterScreen final : public nav::Screen
{
public:
    std::string Name() const override { return {}; }
    const char* DiagName() const override { return "character screen"; }
    bool RemembersCursor() const override { return true; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    const char* TakeSuggestedLanding() override;
    bool TypingIn(const graph::GraphNode& node) override;
    void OnCursorMoved(const graph::GraphNode& node) override;
    void OnPop() override;

private:
    const agui::Widget* _window = nullptr;
    bool _searching = false;    // the search field showed at the last build
    bool _landOnSearch = false; // it has just opened
};

} // namespace fa::screens
