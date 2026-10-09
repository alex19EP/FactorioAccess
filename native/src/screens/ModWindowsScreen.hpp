#pragma once

// The windows mods and the scenario put on the screen (player.gui.screen), the part after what is
// open on Ctrl+Tab (see parts.h), there only while one shows: the sandbox's questions, a mod's
// main window. Sighted players see them over the world and click them whenever they like, so they
// are reached the same way the HUD is, not landed on.
//
// A stop per window, the topmost first, each read in its title's context by the generic walker.
// Their controls are the game's own, pressed as the Gui presses them, so the mod gets its usual
// events. A window that shows up while the player is elsewhere is announced by its title. While the
// part is in use, a label whose text the mod changes is said (the sandbox puts its next question in
// the same label), since it is seldom the focused node.
//
// FactorioAccess's own window there is left out. The game has no window to close here, so Escape
// is ours: it goes back to what is open.

#include <string>
#include <unordered_map>
#include <vector>

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class ModWindowsScreen final : public nav::Screen
{
public:
    std::string Name() const override;
    const char* DiagName() const override { return "mod windows"; }
    bool RemembersCursor() const override { return true; }
    // Over the window it was opened from, as the quickbar is.
    int Layer() const override { return 1; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    bool TypingIn(const graph::GraphNode& node) override;
    void OnCursorMoved(const graph::GraphNode& node) override;
    bool ClaimsEscape() const override { return true; }
    void OnEscape() override;
    void OnPop() override;
    std::string LeaveLine() const override;

private:
    /// Announces the windows in `windows` that were not on screen the frame before.
    void AnnounceNew(const std::vector<const agui::Widget*>& windows);
    /// Says the labels of `windows` whose text changed since the frame before.
    void SayChangedLabels(const std::vector<const agui::Widget*>& windows);

    // Announcement watch, not view state: what was on screen the frame before.
    std::vector<const agui::Widget*> _seen;
    std::unordered_map<const agui::Widget*, std::string> _labels;
};

} // namespace fa::screens
