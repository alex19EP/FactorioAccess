#include "SettingsScreen.hpp"

#include "AguiNodes.hpp"
#include "game.h"

namespace fa::screens
{

using agui::Widget;
using game::layout;

void AddSearchStop(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* search = FindDescendant(window, "SearchBar");
    if (!search)
        return;
    builder.BeginStop("search");
    AddControl(builder, "search/bar", search);
    if (const Widget* popup = FindDescendant(window, "SearchPopup"))
        AddSubtree(builder, "search/popup", popup);
}

void AddSettingsButtons(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* footer = agui::dialogButtons(window);
    const Widget* reset = agui::member(window, layout.settingsReset);
    std::vector<const Widget*> buttons;
    if (Shows(reset) && (!footer || !Contains(footer, reset)))
        buttons.push_back(reset);
    if (footer)
        for (const Widget* button : FindAll(footer, "agui::Button"))
            buttons.push_back(button);
    if (buttons.empty())
        return;
    builder.BeginStop("buttons");
    builder.StartRow();
    for (std::size_t i = 0; i < buttons.size(); ++i)
        AddControl(builder, "buttons/" + std::to_string(i), buttons[i]);
    builder.EndRow();
}

bool SettingsScreen::Handles(const Widget* window) const
{
    return agui::derivesFrom(window, "SettingsGui") && !agui::derivesFrom(window, "ControlSettingsGui")
        && !agui::derivesFrom(window, "ModSettingsGui");
}

void SettingsScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* content = agui::member(window, layout.settingsContent);
    builder.BeginStop("settings");
    AddSubtree(builder, "settings", content, {agui::member(window, layout.settingsReset)});
    AddSettingsButtons(builder, window);
    AddSearchStop(builder, window);
}

} // namespace fa::screens
