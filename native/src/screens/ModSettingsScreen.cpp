#include "ModSettingsScreen.hpp"

#include <span>
#include <string>
#include <vector>

#include "AguiNodes.hpp"
#include "SettingsScreen.hpp"
#include "game.h"

namespace fa::screens
{

namespace
{

using agui::Kind;
using agui::Widget;
using game::layout;

// One mod's settings: a two-column table whose rows are the setting's name (after its reset icon)
// and its control.
void AddMod(graph::GraphBuilder& builder, const Widget* table, const Widget* heading, int& count)
{
    std::string modName = heading ? LabelText(heading) : std::string();
    if (!modName.empty())
        builder.PushContext(modName);
    std::span<const Widget* const> cells = agui::children(table);
    for (std::size_t i = 0; i + 1 < cells.size(); i += 2)
    {
        const Widget* nameCell = cells[i];
        const Widget* control = cells[i + 1];
        if (!Shows(control))
            continue;
        const Widget* label = FindDescendant(nameCell, "agui::Label");
        const Widget* reset = FindDescendant(nameCell, "IconButton");
        std::string key = "settings/" + std::to_string(count++);
        Kind kind = agui::kind(control);
        if (kind == Kind::Container || kind == Kind::HorizontalFlow || kind == Kind::Table)
        {
            // A control made of parts: its parts under the setting's name.
            std::string name = label ? LabelText(label) : std::string();
            if (!name.empty())
                builder.PushContext(name);
            AddSubtree(builder, key, control);
            if (reset && agui::enabled(reset))
                AddControl(builder, key + "/reset", reset);
            if (!name.empty())
                builder.PopContext();
            continue;
        }
        builder.StartRow(key);
        builder.AddItem(graph::ControlId::Referenced(control, key),
            label ? ControlNode(control, label) : ControlNode(control));
        // At its default the reset icon is disabled on every row, which says nothing.
        if (reset && agui::enabled(reset))
            AddControl(builder, key + "/reset", reset);
        builder.EndRow();
    }
    if (!modName.empty())
        builder.PopContext();
}

// The page: notes as text, and each mod's name heading the table of its settings after it.
void AddPage(graph::GraphBuilder& builder, const Widget* widget, int& count)
{
    std::vector<const Widget*> children = VisibleChildren(widget);
    for (std::size_t i = 0; i < children.size(); ++i)
    {
        const Widget* child = children[i];
        switch (agui::kind(child))
        {
        case Kind::Ignored:
            break;
        case Kind::Label:
        {
            bool heading = i + 1 < children.size() && agui::kind(children[i + 1]) == Kind::Table;
            if (!heading)
                builder.AddItem(graph::ControlId::Referenced(child, "settings/text/" + std::to_string(count++)),
                    TextNode(child, [child]() { return LabelText(child); }));
            break;
        }
        case Kind::Table:
        {
            const Widget* heading = i > 0 && agui::kind(children[i - 1]) == Kind::Label ? children[i - 1] : nullptr;
            AddMod(builder, child, heading, count);
            break;
        }
        default:
            AddPage(builder, child, count);
            break;
        }
    }
}

} // namespace

bool ModSettingsScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "ModSettingsGui"); }

void ModSettingsScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* tabs = agui::member(window, layout.modSettingsTabs);
    builder.BeginStop("tabs");
    if (const Widget* header = FindDescendant(tabs, "agui::Table"))
        AddSubtree(builder, "tabs", header);

    builder.BeginStop("settings");
    int count = 0;
    AddPage(builder, agui::member(tabs, layout.tabbedPaneContent), count);

    AddSettingsButtons(builder, window);
    AddSearchStop(builder, window);
}

} // namespace fa::screens
