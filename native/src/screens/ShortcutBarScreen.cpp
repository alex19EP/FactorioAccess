#include "ShortcutBarScreen.hpp"

#include <format>
#include <string>
#include <vector>

#include "AguiNodes.hpp"
#include "parts.h"
#include "text.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;

constexpr const char* kListButtonKey = "list";

std::string ShortcutKey(std::size_t row, std::size_t column) { return std::format("bar/{}/{}", row, column); }

// A shortcut's button, named with the shortcut. One that stays on or off says either state, so that
// pressing it says the new one both ways.
graph::NodeVtable ShortcutNode(const agui::Shortcut& shortcut)
{
    const Widget* button = shortcut.button;
    graph::NodeVtable vtable = ControlNode(button, [name = text::speakable(shortcut.name)]() { return name; });
    if (!shortcut.toggle)
        return vtable;
    auto state = [button]()
    { return std::string(agui::buttonToggled(button) ? vocab::kPressed : vocab::kNotPressed); };
    // The value is the node's first live part.
    for (graph::NodeAnnouncement& announcement : vtable.Announcements)
        if (announcement.Live)
        {
            announcement.Text = state;
            break;
        }
    vtable.StateText = state;
    return vtable;
}

// The shortcut bar, while it is on screen over a loaded game with no menu in front of it.
const Widget* FindBar()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return nullptr;
    const Widget* bar = agui::shortcutBar();
    return bar && agui::visible(bar) ? bar : nullptr;
}

void AddShortcuts(graph::GraphBuilder& builder, const Widget* bar)
{
    std::vector<std::vector<agui::Shortcut>> rows = agui::shortcutBarRows(bar);
    builder.BeginStop("bar");
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        builder.StartRow("bar");
        for (std::size_t j = 0; j < rows[i].size(); ++j)
            builder.AddItem(
                graph::ControlId::Referenced(rows[i][j].button, ShortcutKey(i, j)), ShortcutNode(rows[i][j]));
        builder.EndRow();
    }

    const Widget* listButton = agui::shortcutBarListButton(bar);
    builder.BeginStop("more");
    AddControl(builder, kListButtonKey, listButton,
        [listButton]()
        {
            std::string name = NameOf(listButton);
            return name.empty() ? std::string(vocab::kAllShortcuts) : name;
        });
}

void AddList(graph::GraphBuilder& builder, const std::vector<const Widget*>& boxes)
{
    builder.BeginStop("list");
    for (std::size_t i = 0; i < boxes.size(); ++i)
        builder.AddItem(graph::ControlId::Referenced(boxes[i], std::format("list/{}", i)), ControlNode(boxes[i]));
}

} // namespace

std::string ShortcutBarScreen::Name() const { return vocab::kShortcutBar; }

bool ShortcutBarScreen::IsActive()
{
    if (parts::current() != parts::Part::ShortcutBar)
        return false;
    _bar = FindBar();
    // Nothing to go back to once the game is gone.
    if (!agui::inGame())
        parts::close();
    return _bar != nullptr;
}

void ShortcutBarScreen::Build(graph::GraphBuilder& builder)
{
    if (!_bar || FindBar() != _bar)
        return;
    std::vector<const Widget*> boxes = agui::shortcutBarListCheckBoxes(_bar);
    bool listing = !boxes.empty();
    // Opening the list lands on its first shortcut; closing it back on the button that opened it.
    if (listing && !_listing)
        _landing = "list/0";
    else if (!listing && _listing)
        _landing = kListButtonKey;
    _listing = listing;
    if (listing)
        AddList(builder, boxes);
    else
        AddShortcuts(builder, _bar);
}

const char* ShortcutBarScreen::TakeSuggestedLanding()
{
    if (_landing.empty())
        return nullptr;
    // Copied by the render straight after this call.
    _taken = std::move(_landing);
    _landing.clear();
    return _taken.c_str();
}

void ShortcutBarScreen::OnEscape()
{
    // The list button toggles the list, so pressing it again closes it.
    if (_bar && _listing)
    {
        agui::press(agui::shortcutBarListButton(_bar), agui::MouseButton::Left, false, false);
        return;
    }
    parts::close();
}

void ShortcutBarScreen::OnPop()
{
    _bar = nullptr;
    _listing = false;
    _landing.clear();
}

std::string ShortcutBarScreen::LeaveLine() const
{
    // Left with Ctrl+Tab or Escape, not covered by a menu: the shortcut bar is no longer in use.
    return parts::current() == parts::Part::None ? std::string(vocab::kMap) : std::string();
}

} // namespace fa::screens
