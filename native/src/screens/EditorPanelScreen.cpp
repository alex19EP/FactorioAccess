#include "EditorPanelScreen.hpp"

#include <cstddef>
#include <string>
#include <vector>

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

// The active editor's palette of tiles, entities, resources and the rest: a SelectListGui<T> in the
// panel, whose pick the editor builds with.
void FindPalettes(const Widget* widget, std::vector<const Widget*>& found)
{
    for (const Widget* child : VisibleChildren(widget))
    {
        if (agui::derivesFromTemplate(child, "SelectListGui"))
            found.push_back(child);
        else
            FindPalettes(child, found);
    }
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
    std::vector<const Widget*> palettes;
    FindPalettes(_panel, palettes);
    builder.BeginStop("panel");
    AddSubtree(builder, "panel", _panel, palettes);
    for (std::size_t i = 0; i < palettes.size(); ++i)
    {
        const std::string key = "palette" + std::to_string(i);
        builder.BeginStop(key);
        AddChoices(builder, key + "/", palettes[i]);
    }
}

bool EditorPanelScreen::TypingIn(const graph::GraphNode& node) { return TypingInField(node); }

void EditorPanelScreen::OnCursorMoved(const graph::GraphNode& node) { FollowCursor(node); }

void EditorPanelScreen::OnEscape() { parts::close(); }

void EditorPanelScreen::OnPop()
{
    // The panel stays on screen when left: a field typed in would keep the game's keyboard focus,
    // and with it the keys meant for the map.
    if (_panel && FindPanel() == _panel)
        if (const Widget* focused = agui::focusedWidget(agui::applicationGui()))
            for (const Widget* ancestor = focused; ancestor; ancestor = agui::parent(ancestor))
                if (ancestor == _panel)
                {
                    agui::clearFocus(focused);
                    break;
                }
    _panel = nullptr;
}

std::string EditorPanelScreen::LeaveLine() const
{
    // Left with Ctrl+Tab or Escape, not covered by a menu: the panel is no longer in use.
    return parts::current() == parts::Part::None ? std::string(vocab::kMap) : std::string();
}

} // namespace fa::screens
