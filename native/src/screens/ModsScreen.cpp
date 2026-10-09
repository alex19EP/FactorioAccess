#include "ModsScreen.hpp"

#include <algorithm>
#include <string>

#include "AguiNodes.hpp"
#include "game.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;
using game::layout;

// The first child of the given kind, or null.
const Widget* ChildOfKind(const Widget* widget, agui::Kind kind)
{
    for (const Widget* child : VisibleChildren(widget))
        if (agui::kind(child) == kind)
            return child;
    return nullptr;
}

} // namespace

bool ModsScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "ModsGui"); }

void ModsScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* tabs = agui::member(window, layout.modsTabs);

    builder.BeginStop("tabs");
    if (const Widget* header = ChildOfKind(tabs, agui::Kind::Table))
        AddSubtree(builder, "tabs", header);

    if (agui::tabSelected(agui::member(window, layout.modsManageTab)))
    {
        BuildManage(builder, window);
    }
    else
    {
        builder.BeginStop("content");
        if (const Widget* page = ChildOfKind(tabs, agui::Kind::Container))
            AddSubtree(builder, "content", page);
    }

    AddFooter(builder, window);
}

void ModsScreen::BuildManage(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* pane = agui::member(window, layout.modsManagePane);
    const Widget* table = agui::member(pane, layout.manageModsTable);

    std::span<const Widget* const> cells = agui::children(table);
    std::size_t columns = std::max(agui::tableColumns(table), 1u);
    std::size_t rows = cells.size() / columns;

    builder.BeginStop("mods");
    if (rows > 0)
    {
        if (const Widget* all = FindDescendant(cells[0], "agui::CheckBox"))
            AddControl(builder, "mods/all", all,
                [all]()
                {
                    std::string name = NameOf(all);
                    return name.empty() ? std::string(vocab::kAllMods) : name;
                });
    }
    for (std::size_t row = 1; row < rows; ++row)
    {
        const Widget* enabled = FindDescendant(cells[row * columns], "agui::CheckBox");
        if (!enabled)
            continue;
        // Every other cell of the row is text: the name, the version.
        std::vector<const Widget*> texts;
        for (std::size_t column = 1; column < columns; ++column)
            for (const Widget* label : FindAll(cells[row * columns + column], "agui::Label"))
                texts.push_back(label);
        auto name = [texts]()
        {
            std::string line;
            for (const Widget* label : texts)
            {
                std::string phrase = LabelText(label);
                if (phrase.empty())
                    continue;
                if (!line.empty())
                    line += ' ';
                line += phrase;
            }
            return line;
        };
        if (texts.empty() || !Shows(enabled))
            continue;

        // The name: a click on it selects the mod, as the table picks the row under the mouse.
        auto selected = [table, row]()
        { return agui::selectedRow(table) == row ? std::string(vocab::kSelected) : std::string(); };
        graph::NodeVtable mod = TextNode(texts.front(), name);
        mod.Announcements.emplace_back(selected, true, graph::AnnouncementKinds::Selected);
        const Widget* target = texts.front();
        mod.OnActivate = [table, target]() { agui::pressOver(table, target, agui::MouseButton::Left, false, false); };
        mod.StateText = selected;

        std::string key = "mods/" + std::to_string(row);
        builder.StartRow("mod");
        builder.AddItem(graph::ControlId::Referenced(target, key), std::move(mod));
        // Then its checkbox, which enables or disables it.
        AddControl(builder, key + "/enabled", enabled);
        builder.EndRow();
        if (agui::selectedRow(table) == row)
            builder.SetStart(graph::ControlId::Structural(key));
    }

    builder.BeginStop("info");
    AddSubtree(builder, "info", agui::member(pane, layout.manageModsInfo));

    builder.BeginStop("search");
    const Widget* search = agui::member(pane, layout.manageModsSearch);
    AddControl(builder, "search/bar", search);
    if (const Widget* popup = FindDescendant(pane, "SearchPopup"))
        AddSubtree(builder, "search/popup", popup);
    // The header's sort buttons, named by the column they sort.
    for (std::size_t column = 0; rows > 0 && column < columns; ++column)
    {
        const Widget* sort = FindDescendant(cells[column], "agui::TextButton");
        const Widget* caption = FindDescendant(cells[column], "agui::Label");
        if (!sort || !caption)
            continue;
        AddControl(builder, "search/sort" + std::to_string(column), sort,
            [caption]() { return vocab::kSortBy(LabelText(caption)); });
    }
}

} // namespace fa::screens
