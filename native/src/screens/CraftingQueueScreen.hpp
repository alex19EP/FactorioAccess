#pragma once

// The game's crafting queue at the bottom left (CraftingQueueGui), the part of the HUD after the
// status on Ctrl+Tab (see parts.h).
//
// One stop: a row of the queue's slots, the order being crafted first, each read as its recipe and
// count. Past two rows of slots the game ends them with a button that shows every order, or two rows
// again; it ends the row here too. With nothing queued the row says so.
//
// Every slot is the game's own button, clicked as the Gui clicks it, so Enter cancels one craft as a
// click does in vanilla, and the other clicks cancel five or all as the game's controls bind them.
//
// The game has no window to close here, so Escape is ours: it goes back to what is open.

#include <string>

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class CraftingQueueScreen final : public nav::Screen
{
public:
    const char* Name() const override;
    const char* DiagName() const override { return "crafting queue"; }
    // Back on the slot it was left on, after Ctrl+Tab went to the window and came back.
    bool RemembersCursor() const override { return true; }
    // Over the window it was opened from, as the quickbar is.
    int Layer() const override { return 1; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    bool ClaimsEscape() const override { return true; }
    void OnEscape() override;
    void OnPop() override;
    std::string LeaveLine() const override;

private:
    const agui::Widget* _queue = nullptr;
};

} // namespace fa::screens
