#pragma once

// The tips and tricks window (TipsAndTricksGui), opened from the side menu or the "New tip" button.
// It pauses a single player game while open.
//
// Stops, read in the window title's context:
//   - search: the title bar's search field while open (opening it lands there); typing filters
//     the list as in vanilla;
//   - list: the tips in the game's order, each its name, then "new" for a suggested tip and
//     "selected" for the tip shown; the game hides locked tips. A title tip heads the tips
//     indented under it; Ctrl+Up and Ctrl+Down jump from heading to heading, as between the
//     sections of the controls settings. Nothing folds: the game shows every tip it unlocked. Enter
//     clicks the tip as the mouse would, which shows it (and marks it read, unlocking the tips
//     under it), and lands on its title in the page;
//   - page: the tip's title, then its description a line each, the icons the game makes clickable
//     in a line beside it as links, then Play tutorial (Replay tutorial) and Mark as unread. With
//     nothing found, the game's "No tips and tricks selected" and its note on new tips instead.
//     The animation or picture over the description has nothing to read;
//   - controls: the title bar's search button, Back, Forward and close.
//
// Whenever another tip shows (chosen in the list, or the game's Back and Forward controls, Alt+Left
// and Alt+Right), the cursor lands on its title. Escape closes the window as in vanilla.

#include <string>

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class TipsAndTricksScreen final : public EntityWindowScreen
{
public:
    const char* TakeSuggestedLanding() override;
    void OnPop() override;

protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;

private:
    std::string _title;             // the tip whose page showed at the last build
    bool _searching = false;        // the search field showed at the last build
    const char* _landing = nullptr; // where the next render lands, once
};

} // namespace fa::screens
