#include "EditorPanelScreen.hpp"

#include <string>

#include "AguiNodes.hpp"
#include "parts.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;

// The editor panel, while it is on screen over a loaded game with no menu in front of it.
const Widget* FindPanel()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return nullptr;
    return agui::mapEditorPanel();
}

} // namespace

std::string EditorPanelScreen::Name() const { return vocab::kEditorPanel; }

bool EditorPanelScreen::IsActive()
{
    _panel = FindPanel();
    parts::setAvailable(parts::Part::EditorPanel, _panel != nullptr);
    if (parts::current() != parts::Part::EditorPanel)
        return false;
    // Nothing to go back to once the game is gone.
    if (!agui::inGame())
        parts::close();
    return _panel != nullptr;
}

void EditorPanelScreen::Build(graph::GraphBuilder& builder)
{
    if (!_panel || FindPanel() != _panel)
        return;
    builder.BeginStop("panel");
    AddSubtree(builder, "panel", _panel);
}

void EditorPanelScreen::OnEscape() { parts::close(); }

void EditorPanelScreen::OnPop() { _panel = nullptr; }

std::string EditorPanelScreen::LeaveLine() const
{
    // Left with Ctrl+Tab or Escape, not covered by a menu: the panel is no longer in use.
    return parts::current() == parts::Part::None ? std::string(vocab::kMap) : std::string();
}

} // namespace fa::screens
