#include "AchievementCards.hpp"

#include <cmath>
#include <format>
#include <string>
#include <vector>

#include "speech.h"
#include "text.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Kind;
using agui::Widget;
namespace Kinds = graph::AnnouncementKinds;

std::string BarText(const Widget* bar)
{
    std::string caption = text::speakable(agui::text(bar));
    return caption.empty() ? std::format("{}%", std::lround(agui::progress(bar) * 100)) : caption;
}

// The texts of a card as the game shows them, a line per label or bar; a row of them is one line.
void CollectLines(const Widget* widget, std::vector<std::string>& lines)
{
    for (const Widget* child : VisibleChildren(widget))
    {
        switch (agui::kind(child))
        {
        case Kind::Label:
            if (std::string line = LabelText(child); !line.empty())
                lines.push_back(std::move(line));
            break;
        case Kind::ProgressBar:
            lines.push_back(BarText(child));
            break;
        case Kind::HorizontalFlow:
        {
            std::vector<std::string> parts;
            CollectLines(child, parts);
            std::string line;
            for (const std::string& part : parts)
                line += (line.empty() ? "" : " ") + part;
            if (!line.empty())
                lines.push_back(std::move(line));
            break;
        }
        default:
            CollectLines(child, lines);
            break;
        }
    }
}

// What the card's frame shows; in the window, also whether a normal card's track button is down.
// Every card on the HUD is tracked, so there it goes unsaid.
std::string StateText(const Widget* card, bool hud)
{
    agui::AchievementCard parts = agui::achievementCard(card);
    switch (parts.state)
    {
    case agui::AchievementState::Earned:
        return std::string(vocab::kEarned);
    case agui::AchievementState::Failed:
        return std::string(vocab::kFailed);
    case agui::AchievementState::Normal:
        break;
    }
    return !hud && parts.track && agui::buttonToggled(parts.track) ? std::string(vocab::kTracked) : std::string();
}

std::string CardText(const Widget* card, bool hud)
{
    std::vector<std::string> lines;
    // A HUD card shows the achievement's icon, named only by the card's tooltip.
    if (hud)
        if (std::string name = text::speakable(agui::toolTip(card).title); !name.empty())
            lines.push_back(std::move(name));
    CollectLines(agui::achievementCard(card).description, lines);
    std::string text;
    for (const std::string& line : lines)
        text += (text.empty() ? "" : ", ") + line;
    return text;
}

// The tooltip the card's right side shows: a failed card's reason, the track button's action; on
// the HUD the card's own, the achievement's name.
std::string TooltipText(const Widget* card, bool hud)
{
    agui::AchievementCard parts = agui::achievementCard(card);
    const Widget* tipped = hud ? card : parts.warning ? parts.warning : parts.track;
    return tipped ? text::speakable(agui::toolTip(tipped).title) : std::string();
}

graph::NodeVtable CardNode(const Widget* card, bool hud)
{
    graph::NodeVtable vtable;
    vtable.HostTag = card;
    vtable.Announcements.emplace_back([card, hud]() { return StateText(card, hud); }, false, Kinds::Value);
    vtable.Announcements.emplace_back([card, hud]() { return CardText(card, hud); }, false, Kinds::Label);
    if (!TooltipText(card, hud).empty())
        vtable.OnTooltip = [card, hud]() { speech::say(TooltipText(card, hud), true); };
    if (agui::achievementCard(card).track && hud)
    {
        // A HUD card's button only stops tracking, and takes the card away with it.
        vtable.OnActivate = [card]()
        {
            agui::press(agui::achievementCard(card).track, agui::MouseButton::Left, false, false);
            speech::say(std::string(vocab::kNotTracked), true);
        };
    }
    else if (agui::achievementCard(card).track)
    {
        // The game remakes the button on every click, so it is looked up again each time.
        vtable.OnActivate = [card]()
        {
            if (const Widget* track = agui::achievementCard(card).track)
                agui::press(track, agui::MouseButton::Left, false, false);
        };
        vtable.StateText = [card]()
        {
            const Widget* track = agui::achievementCard(card).track;
            if (!track)
                return std::string();
            return std::string(agui::buttonToggled(track) ? vocab::kTracked : vocab::kNotTracked);
        };
    }
    return vtable;
}

} // namespace

void AddAchievementCards(graph::GraphBuilder& builder, const std::string& prefix, const Widget* holder, bool hud)
{
    bool any = false;
    for (const Widget* card : FindAll(holder, "AchievementCard"))
    {
        std::string key = std::format("{}/{}", prefix, agui::achievementCard(card).prototype);
        builder.AddItem(graph::ControlId::Referenced(card, key), CardNode(card, hud));
        any = true;
    }
    if (!any)
        builder.AddLabel(graph::ControlId::Structural(prefix + "/empty"), []() { return std::string(vocab::kEmpty); });
}

} // namespace fa::screens
