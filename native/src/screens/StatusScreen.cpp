#include "StatusScreen.hpp"

#include <format>
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

// Replaces a node's value, its first live part, with `text`, read when the node is reached rather
// than watched: research progress and counts move too often to be spoken on every change.
void SetValue(graph::NodeVtable& vtable, std::function<std::string()> text)
{
    for (graph::NodeAnnouncement& announcement : vtable.Announcements)
        if (announcement.Live)
        {
            announcement.Text = std::move(text);
            announcement.Live = false;
            break;
        }
}

bool Shown() { return agui::inGame() && !agui::menuStateWindow(); }

} // namespace

std::string StatusScreen::Name() const { return vocab::kStatus; }

bool StatusScreen::IsActive()
{
    if (parts::current() != parts::Part::Status)
        return false;
    // Nothing to go back to once the game is gone.
    if (!agui::inGame())
        parts::close();
    return Shown();
}

void StatusScreen::Open(const Widget* button)
{
    agui::press(button, agui::MouseButton::Left, false, false);
    _opening = true;
    parts::close();
}

double StatusScreen::AlertCount(const Widget* button)
{
    double count = agui::iconButtonCount(button);
    if (count > 0)
        _alertCounts[button] = count;
    return _alertCounts[button];
}

void StatusScreen::Build(graph::GraphBuilder& builder)
{
    if (!Shown())
        return;

    agui::ResearchBox research = agui::researchBox();
    if (research.button)
    {
        builder.BeginStop("research");
        graph::NodeVtable vtable = ControlNode(research.button, [title = research.title]() { return LabelText(title); });
        if (const Widget* progress = research.progress)
            SetValue(vtable, [progress]() { return LabelText(progress); });
        vtable.OnActivate = [this, button = research.button]() { Open(button); };
        builder.AddItem(graph::ControlId::Referenced(research.button, "research"), std::move(vtable));
    }

    std::vector<agui::AlertButton> alerts = agui::alertButtons();
    if (!alerts.empty())
    {
        builder.BeginStop("alerts");
        builder.PushContext(std::string(vocab::kAlerts));
        for (const agui::AlertButton& alert : alerts)
        {
            graph::NodeVtable vtable =
                ControlNode(alert.button, [category = static_cast<uint8_t>(alert.category)]()
                    { return vocab::alertCategory(category); });
            SetValue(vtable, [this, button = alert.button]() { return std::format("{:.0f}", AlertCount(button)); });
            vtable.OnActivate = [this, button = alert.button]() { Open(button); };
            builder.AddItem(graph::ControlId::Referenced(
                                alert.button, std::format("alerts/{}", static_cast<int>(alert.category))),
                std::move(vtable));
        }
        builder.PopContext();
    }

    if (const Widget* goal = agui::goalLabel())
    {
        builder.BeginStop("goal");
        builder.PushContext(std::string(vocab::kGoal));
        builder.AddItem(graph::ControlId::Referenced(goal, "goal"), TextNode(goal, [goal]() { return LabelText(goal); }));
        builder.PopContext();
    }

    agui::HudBars bars = agui::hudBars();
    std::pair<const Widget*, vocab::Word> named[] = {
        {bars.health, vocab::kHealth},
        {bars.shield, vocab::kShield},
        {bars.vehicleHealth, vocab::kVehicleHealth},
        {bars.vehicleShield, vocab::kVehicleShield},
        {bars.mining, vocab::kMining},
    };
    builder.BeginStop("bars");
    for (const auto& [bar, name] : named)
        if (bar)
            builder.AddItem(graph::ControlId::Referenced(bar, std::format("bars/{}", name.key)),
                ControlNode(bar, [name]() { return std::string(name); }));
}

void StatusScreen::OnEscape() { parts::close(); }

void StatusScreen::OnPush() { _opening = false; }

std::string StatusScreen::LeaveLine() const
{
    // Left with Ctrl+Tab or Escape, not covered by a menu and not for a window it opened: the status
    // is no longer in use.
    return parts::current() == parts::Part::None && !_opening ? std::string(vocab::kMap) : std::string();
}

} // namespace fa::screens
