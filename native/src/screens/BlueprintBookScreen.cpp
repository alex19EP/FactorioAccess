#include "BlueprintBookScreen.hpp"

#include <span>
#include <string>
#include <utility>
#include <vector>

#include "AguiNodes.hpp"
#include "game.h"

namespace fa::screens
{

namespace
{

using agui::Widget;
using game::layout;

// The name with the rename button, the description, then the way up: the navigation flow's rows
// before the book's own, each an arrow beside the name of where it goes.
void AddBook(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* name = agui::member(window, layout.bookGuiName);
    const Widget* rename = agui::member(window, layout.bookGuiRename);
    const Widget* description = agui::member(window, layout.bookGuiDescription);
    builder.BeginStop("book");
    builder.StartRow();
    if (Shows(name))
        builder.AddItem(graph::ControlId::Referenced(name, "book/name"), TextNode(name, [name]() { return LabelText(name); }));
    AddControl(builder, "book/rename", rename);
    builder.EndRow();
    if (Shows(description) && !LabelText(description).empty())
        builder.AddItem(graph::ControlId::Referenced(description, "book/description"),
            TextNode(description, [description]() { return LabelText(description); }));
    int level = 0;
    for (const Widget* row : VisibleChildren(agui::member(window, layout.bookGuiNavigation)))
    {
        if (Contains(row, name))
            break;
        std::vector<const Widget*> arrows = FindAll(row, "agui::Button");
        std::vector<const Widget*> labels = FindAll(row, "agui::Label");
        if (arrows.empty() || labels.empty())
            continue;
        builder.AddItem(graph::ControlId::Referenced(arrows.front(), "book/up/" + std::to_string(level++)),
            ControlNode(arrows.front(), labels.front()));
    }
}

// The header's subheader: the book's item name as its caption, then its buttons.
void AddButtons(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* subheader = agui::member(agui::member(window, layout.bookGuiHeader), layout.frameSubheader);
    builder.BeginStop("buttons");
    builder.StartRow();
    int index = 0;
    for (const Widget* button : FindAll(subheader, "agui::Button"))
        AddControl(builder, "buttons/" + std::to_string(index++), button);
    builder.EndRow();
}

// The list's cells: in List view a line between items and a row per item (the slot, its name and
// its description); in Grid view the slot over its name; in Slots view the slot alone. The slots
// are keyed by their place in the book, so the cursor keeps its slot when the view changes.
void AddContents(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* list = agui::member(window, layout.bookGuiList);
    if (!Shows(list))
        return;
    // In List view the table's columns hold the lines between items too; an item is a row.
    bool listView = agui::blueprintsListView(list);
    unsigned columns = listView ? 1 : agui::tableColumns(list);
    std::span<const Widget* const> cells = agui::children(list);
    if (columns == 0)
        return;
    builder.BeginStop("contents");
    int slotNumber = 0;
    for (std::size_t start = 0; start < cells.size(); start += columns)
    {
        bool rowStarted = false;
        for (std::size_t i = start; i < cells.size() && i < start + columns; ++i)
        {
            const Widget* slot = agui::visible(cells[i]) ? FindDescendant(cells[i], "BlueprintBookSlot") : nullptr;
            if (!slot)
                continue;
            graph::NodeVtable node = ControlNode(slot);
            if (listView)
            {
                std::vector<const Widget*> labels = FindAll(cells[i], "agui::Label");
                if (labels.size() > 1)
                {
                    const Widget* description = labels[1];
                    node.Announcements.emplace_back([description]() { return LabelText(description); }, false,
                        graph::AnnouncementKinds::Value);
                }
            }
            if (!rowStarted)
            {
                builder.StartRow("contents");
                rowStarted = true;
            }
            builder.AddItem(graph::ControlId::Referenced(slot, "contents/" + std::to_string(slotNumber++)), std::move(node));
        }
        if (rowStarted)
            builder.EndRow();
    }
}

// The inside frame's subheader: the hint on cycling through the book, then the view buttons.
void AddView(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* subheader = agui::member(agui::member(window, layout.bookGuiInside), layout.frameSubheader);
    builder.BeginStop("view");
    int index = 0;
    for (const Widget* label : FindAll(subheader, "agui::Label"))
        if (!LabelText(label).empty())
            builder.AddItem(graph::ControlId::Referenced(label, "view/hint/" + std::to_string(index++)),
                TextNode(label, [label]() { return LabelText(label); }));
    builder.StartRow();
    index = 0;
    for (const Widget* button : FindAll(subheader, "agui::Button"))
        AddControl(builder, "view/" + std::to_string(index++), button);
    builder.EndRow();
}

} // namespace

bool BlueprintBookScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "BlueprintBookGui"); }

void BlueprintBookScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    AddBook(builder, window);
    AddButtons(builder, window);
    AddContents(builder, window);
    AddView(builder, window);
    AddInventory(builder, agui::entityWindowParts(window));
}

} // namespace fa::screens
