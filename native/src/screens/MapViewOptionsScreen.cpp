#include "MapViewOptionsScreen.hpp"

#include <string>

#include "AguiNodes.hpp"
#include "parts.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;

// The map view options, while they are on screen over a loaded game with no menu in front of them.
const Widget* FindOptions()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return nullptr;
    return agui::mapViewOptions();
}

// The panel above them with the drop to a planet button, while it shows anything.
const Widget* FindPanel()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return nullptr;
    const Widget* panel = agui::remoteViewPanel();
    return panel && HasContent(panel) ? panel : nullptr;
}

} // namespace

std::string MapViewOptionsScreen::Name() const { return vocab::kMapViewOptions; }

bool MapViewOptionsScreen::IsActive()
{
    _options = FindOptions();
    _panel = FindPanel();
    const bool shown = _options || _panel;
    parts::setAvailable(parts::Part::MapViewOptions, shown);
    if (parts::current() != parts::Part::MapViewOptions)
        return false;
    // Nothing to go back to once the game is gone.
    if (!agui::inGame())
        parts::close();
    return shown;
}

void MapViewOptionsScreen::Build(graph::GraphBuilder& builder)
{
    if (_panel && FindPanel() == _panel)
    {
        builder.BeginStop("landing");
        builder.StartRow();
        int index = 0;
        for (const Widget* button : FindAll(_panel, "agui::Button"))
        {
            if (!Shows(button))
                continue;
            graph::NodeVtable vtable = ControlNode(button);
            // The drop button opens the planet list, which takes over from the HUD; cancelling acts
            // in place.
            if (agui::derivesFrom(button, "agui::TextButton"))
                vtable.OnActivate = [this, button]()
                {
                    if (!agui::enabled(button))
                        return;
                    agui::press(button, agui::MouseButton::Left, false, false);
                    _opening = true;
                    parts::close();
                };
            builder.AddItem(
                graph::ControlId::Referenced(button, "landing/" + std::to_string(index++)), std::move(vtable));
        }
        builder.EndRow();
    }
    if (_options && FindOptions() == _options)
    {
        builder.BeginStop("options");
        AddSubtree(builder, "options", _options);
    }
}

void MapViewOptionsScreen::OnEscape() { parts::close(); }

void MapViewOptionsScreen::OnPush() { _opening = false; }

void MapViewOptionsScreen::OnPop()
{
    _options = nullptr;
    _panel = nullptr;
}

std::string MapViewOptionsScreen::LeaveLine() const
{
    // Left with Ctrl+Tab or Escape, not covered by a menu and not for the planet list: the options
    // are no longer in use.
    return parts::current() == parts::Part::None && !_opening ? std::string(vocab::kMap) : std::string();
}

} // namespace fa::screens
