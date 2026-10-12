#pragma once

// The Save game window over a loaded game (SaveMapGui). Stops:
//   saves   — the save list, read without touching the game; the window opens on it. Enter clicks
//             a save, selecting it, and the game puts its name in the name field; Enter on the
//             selected save saves over it, as a double-click does. Backspace on the selected save
//             asks to delete it
//   name    — the "Save as" field; Enter in it saves
//   details — the selected save's map info
//   buttons — Back and Save
//   search  — the search field and the sort order buttons
//
// The game focuses the name field as the window opens; typing goes there only once the cursor is
// on it. The cursor is not remembered: every opening starts on the saves.

#include "WindowScreen.hpp"

namespace fa::screens
{

class SaveGameScreen final : public WindowScreen
{
public:
    bool Handles(const agui::Widget* window) const override;
    bool RemembersCursor() const override { return false; }

protected:
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
