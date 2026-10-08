#include "AchievementsScreen.hpp"

#include <cmath>
#include <format>
#include <string>
#include <vector>

#include "game.h"
#include "text.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Kind;
using agui::Widget;
using game::layout;
namespace Kinds = graph::AnnouncementKinds;

constexpr const char* kSearchKey = "search/field";

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

std::string Join(const std::vector<std::string>& lines)
{
    std::string text;
    for (const std::string& line : lines)
        text += (text.empty() ? "" : ", ") + line;
    return text;
}

// What the card's frame shows, and for a normal card whether its track button is down.
std::string StateText(const agui::AchievementCard& card)
{
    switch (card.state)
    {
    case agui::AchievementState::Earned:
        return std::string(vocab::kEarned);
    case agui::AchievementState::Failed:
        return std::string(vocab::kFailed);
    case agui::AchievementState::Normal:
        break;
    }
    return card.track && agui::buttonToggled(card.track) ? std::string(vocab::kTracked) : std::string();
}

graph::NodeVtable CardNode(const Widget* card)
{
    graph::NodeVtable vtable;
    vtable.HostTag = card;
    vtable.Announcements.emplace_back([card]() { return StateText(agui::achievementCard(card)); }, false, Kinds::Value);
    vtable.Announcements.emplace_back(
        [card]()
        {
            std::vector<std::string> lines;
            CollectLines(agui::achievementCard(card).description, lines);
            return Join(lines);
        },
        false, Kinds::Label);
    if (agui::achievementCard(card).track)
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

void AddSummary(graph::GraphBuilder& builder, const Widget* window)
{
    builder.BeginStop("summary");
    const Widget* label = agui::member(window, layout.achievementsProgress);
    const Widget* bar = agui::member(window, layout.achievementsBar);
    if (Shows(label) && !LabelText(label).empty())
        builder.AddItem(graph::ControlId::Referenced(label, "summary/progress"),
            TextNode(label, [label, bar]() { return std::format("{}, {}", LabelText(label), BarText(bar)); }));
    for (const auto& [offset, key] : {std::pair{layout.achievementsModded, "summary/modded"},
             std::pair{layout.achievementsPlaytime, "summary/playtime"}})
    {
        const Widget* note = agui::member(window, offset);
        if (Shows(note) && !LabelText(note).empty())
            builder.AddItem(graph::ControlId::Referenced(note, key),
                TextNode(note, [note]() { return LabelText(note); }));
    }
}

void AddSearch(graph::GraphBuilder& builder, const Widget* window, const Widget* field)
{
    builder.BeginStop("search");
    AddControl(builder, "search/button", FindDescendant(window, "SearchBar"));
    if (field)
        builder.AddItem(graph::ControlId::Referenced(field, kSearchKey), ControlNode(field));
}

// Keyed by achievement, so that the cursor stays on one as the search hides others.
void AddCards(graph::GraphBuilder& builder, const Widget* window)
{
    builder.BeginStop("achievements");
    bool any = false;
    for (const Widget* card : FindAll(agui::member(window, layout.achievementsHolder), "AchievementCard"))
    {
        std::string key = std::format("achievements/{}", agui::achievementCard(card).prototype);
        builder.AddItem(graph::ControlId::Referenced(card, key), CardNode(card));
        any = true;
    }
    if (!any)
        builder.AddLabel(graph::ControlId::Structural("achievements/empty"),
            []() { return std::string(vocab::kEmpty); });
}

} // namespace

bool AchievementsScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "AchievementGui"); }

void AchievementsScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* title = agui::frameTitle(window);
    std::string context = title ? text::speakable(agui::text(title)) : std::string();
    if (!context.empty())
        builder.PushContext(context);

    const Widget* field = FindDescendant(window, "agui::TextField");
    // Opening the search lands on its field.
    if (field && !_searching)
        _landing = kSearchKey;
    _searching = field != nullptr;

    AddSummary(builder, window);
    AddSearch(builder, window, field);
    AddCards(builder, window);

    if (!context.empty())
        builder.PopContext();
}

const char* AchievementsScreen::TakeSuggestedLanding()
{
    const char* landing = _landing;
    _landing = nullptr;
    return landing;
}

void AchievementsScreen::OnPop()
{
    EntityWindowScreen::OnPop();
    _searching = false;
    _landing = nullptr;
}

} // namespace fa::screens
