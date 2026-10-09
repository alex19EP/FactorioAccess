#include "AchievementsScreen.hpp"

#include <format>
#include <string>

#include "AchievementCards.hpp"
#include "game.h"
#include "text.h"

namespace fa::screens
{

namespace
{

using agui::Widget;
using game::layout;

constexpr const char* kSearchKey = "search/field";

void AddSummary(graph::GraphBuilder& builder, const Widget* window)
{
    builder.BeginStop("summary");
    const Widget* label = agui::member(window, layout.achievementsProgress);
    const Widget* bar = agui::member(window, layout.achievementsBar);
    if (Shows(label) && !LabelText(label).empty())
        builder.AddItem(graph::ControlId::Referenced(label, "summary/progress"),
            TextNode(label, [label, bar]()
                { return std::format("{}, {}", LabelText(label), text::speakable(agui::text(bar))); }));
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
    builder.BeginStop("achievements");
    AddAchievementCards(builder, "achievements", agui::member(window, layout.achievementsHolder), false);

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
