#pragma once

// The map editor's panel at the left (MapEditorGui), the part of the HUD after the mod windows on
// Ctrl+Tab (see parts.h), there only in the map editor.
//
// One stop, as the game lays it out: the Editors and Settings tabs; under Editors the table of
// editor buttons (entities, tiles, resources, forces, surfaces, time, cloning, scripting and the
// rest) over the active editor's tools and brush settings; under Settings the editor's checkboxes.
// Each is the game's own widget, clicked as the Gui clicks it.
//
// The game has no window to close here, so Escape is ours: it goes back to what is open.

#include <string>

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class EditorPanelScreen final : public nav::Screen
{
public:
    std::string Name() const override;
    const char* DiagName() const override { return "editor panel"; }
    // Back on the control it was left on, after Ctrl+Tab went to the map and came back.
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
    const agui::Widget* _panel = nullptr;
};

} // namespace fa::screens
