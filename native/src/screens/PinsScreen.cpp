#include "PinsScreen.hpp"

#include <format>
#include <string>

#include "AguiNodes.hpp"
#include "parts.h"
#include "text.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;

// The pins panel, while it is on screen over a loaded game with no menu in front of it.
const Widget* FindPanel()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return nullptr;
    return agui::pinsPanel();
}

std::string RowKey(std::size_t index) { return std::format("pins/{}", index); }

std::string NameText(const Widget* name) { return text::speakable(agui::text(name)); }

} // namespace

std::string PinsScreen::Name() const { return vocab::kPins; }

bool PinsScreen::IsActive()
{
    _panel = FindPanel();
    parts::setAvailable(parts::Part::Pins, _panel != nullptr);
    if (parts::current() != parts::Part::Pins)
        return false;
    // Nothing to go back to once the game is gone, and the panel goes with the last pin.
    if (!agui::inGame() || !_panel)
        parts::close();
    if (!_panel)
        return false;
    // A pin's settings show beside the panel while its edit button stays pressed; that window's own
    // screen reads them, and the pins come back once it closes.
    for (const agui::PinRow& row : agui::pinRows(_panel).rows)
        if (agui::buttonToggled(row.edit))
            return false;
    return true;
}

void PinsScreen::Build(graph::GraphBuilder& builder)
{
    if (!_panel || FindPanel() != _panel)
        return;
    agui::PinRows pins = agui::pinRows(_panel);
    builder.BeginStop("pins");
    // Keyed by place, as the game rebuilds every row when the pins change: removing a pin leaves the
    // cursor on the one that took its place.
    for (std::size_t i = 0; i < pins.rows.size(); ++i)
    {
        const agui::PinRow& row = pins.rows[i];
        std::string key = RowKey(i);
        builder.StartLine(key);

        graph::NodeVtable name = ControlNode(row.name, [widget = row.name]() { return NameText(widget); });
        const Widget* list = pins.list;
        const Widget* moved = row.row;
        std::string said = NameText(row.name);
        name.OnReorder = [this, list, moved, i, said](int by)
        {
            if (!agui::dragChild(list, moved, by))
                return false;
            auto to = static_cast<std::size_t>(static_cast<int>(i) + by);
            // The game moves the pin on its next update and rebuilds the rows; the cursor follows it
            // there.
            _landing = Landing{RowKey(to) + "/name",
                [to, said]()
                {
                    const Widget* panel = FindPanel();
                    if (!panel)
                        return false;
                    agui::PinRows now = agui::pinRows(panel);
                    return to < now.rows.size() && NameText(now.rows[to].name) == said;
                },
                120};
            return true;
        };
        builder.AddItem(graph::ControlId::Referenced(row.name, key + "/name"), std::move(name));
        builder.AddItem(graph::ControlId::Referenced(row.edit, key + "/edit"),
            ControlNode(row.edit, []() { return std::string(vocab::kEdit); }));
        graph::NodeVtable remove = ControlNode(row.remove, []() { return std::string(vocab::kRemove); });
        std::size_t count = pins.rows.size();
        remove.OnActivate = [this, widget = row.remove, i, count]()
        {
            agui::press(widget, agui::MouseButton::Left, false, false);
            // The last pin takes the panel with it, and the part closes.
            if (count < 2)
                return;
            // The cursor goes to the pin that takes the place, or the one before the last, said as
            // the game rebuilds the rows: another Enter on the same spot would remove that one too.
            std::size_t to = i + 1 < count ? i : i - 1;
            _landing = Landing{RowKey(to) + "/name",
                [count]()
                {
                    const Widget* panel = FindPanel();
                    return panel && agui::pinRows(panel).rows.size() < count;
                },
                120};
        };
        builder.AddItem(graph::ControlId::Referenced(row.remove, key + "/remove"), std::move(remove));
        builder.EndRow();
    }
}

const char* PinsScreen::TakeSuggestedLanding()
{
    if (!_landing)
        return nullptr;
    // Builds run every frame and after every key; 120 is about two seconds.
    if (!_panel || --_landing->builds <= 0)
    {
        _landing.reset();
        return nullptr;
    }
    if (!_landing->ready())
        return nullptr;
    _landed = std::move(_landing->key);
    _landing.reset();
    return _landed.c_str();
}

void PinsScreen::OnEscape() { parts::close(); }

void PinsScreen::OnPop()
{
    _panel = nullptr;
    _landing.reset();
}

std::string PinsScreen::LeaveLine() const
{
    // Left with Ctrl+Tab or Escape, not covered by a menu: the pins are no longer in use.
    return parts::current() == parts::Part::None ? std::string(vocab::kMap) : std::string();
}

} // namespace fa::screens
