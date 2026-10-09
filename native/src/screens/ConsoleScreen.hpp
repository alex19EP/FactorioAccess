#pragma once

// The game's console, opened by its toggle key (the grave key): the input field at the bottom of the
// screen, and above it the log of what was said and printed, which the open console shows up to the
// top of the screen.
//
// Three stops. Tab completes and Up and Down recall history in the field, as in vanilla, so
// Ctrl+Tab and Ctrl+Shift+Tab move between them:
// - the field, where the cursor lands. Typing, Enter (sends the line and closes the console) and
//   Escape (closes it) are the game's own;
// - the icon button inside it, whose chooser adds an icon to the line;
// - the log, a line per message as the game draws it (the speaker's name, then the message), the
//   oldest at the top; it is entered on the newest. Right steps through a line's icons: Enter
//   clicks one as the mouse does (a map position closes the console and opens the map there, a
//   blueprint closes it and takes the blueprint in hand), Y reads its tooltip.
//
// The console takes the keys while it is open: the player closes it to play again. (Sighted players
// can click past it and play on with it open; FA does not do that.)

#include <cstddef>

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class ConsoleScreen final : public nav::Screen
{
public:
    std::string Name() const override;
    const char* DiagName() const override { return "console"; }
    // Over the map, the HUD and any window it was opened over.
    int Layer() const override { return 1; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    const char* TakeSuggestedLanding() override;
    bool TypingIn(const graph::GraphNode& node) override;
    bool CommandLine() const override { return true; }
    void OnCursorMoved(const graph::GraphNode& node) override;
    void OnPush() override;
    void OnPop() override;
    std::string LeaveLine() const override;

private:
    const agui::Widget* _field = nullptr;
    bool _landOnField = false;
    // A click on a log line's icon, run at the next poll: it may close the console, which frees the
    // field the render points at.
    const void* _clickItem = nullptr;
    std::size_t _clickSection = 0;
};

} // namespace fa::screens
