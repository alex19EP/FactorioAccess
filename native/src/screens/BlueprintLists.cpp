#include "BlueprintLists.hpp"

#include <span>
#include <utility>
#include <vector>

#include "game.h"

namespace fa::screens
{

using agui::Widget;

namespace
{

// The slot a cell of a BlueprintsList holds: a book's, or a library record's.
const Widget* SlotIn(const Widget* cell)
{
    if (!agui::visible(cell))
        return nullptr;
    if (const Widget* slot = FindDescendant(cell, "BlueprintBookSlot"))
        return slot;
    return FindDescendant(cell, "BlueprintRecordSlotButton");
}

} // namespace

void AddBook(graph::GraphBuilder& builder, const BookParts& book)
{
    builder.BeginStop("book");
    builder.StartRow();
    if (Shows(book.name))
        builder.AddItem(graph::ControlId::Referenced(book.name, "book/name"),
            TextNode(book.name, [name = book.name]() { return LabelText(name); }));
    AddControl(builder, "book/rename", book.rename);
    builder.EndRow();
    if (Shows(book.description) && !LabelText(book.description).empty())
        builder.AddItem(graph::ControlId::Referenced(book.description, "book/description"),
            TextNode(book.description, [description = book.description]() { return LabelText(description); }));
    // The rows may sit in a flow of their own inside the navigation widget.
    std::vector<const Widget*> rows = VisibleChildren(book.navigation);
    while (rows.size() == 1)
        rows = VisibleChildren(rows.front());
    int level = 0;
    for (const Widget* row : rows)
    {
        if (Contains(row, book.name))
            break;
        std::vector<const Widget*> arrows = FindAll(row, "agui::Button");
        std::vector<const Widget*> labels = FindAll(row, "agui::Label");
        if (arrows.empty() || labels.empty())
            continue;
        builder.AddItem(graph::ControlId::Referenced(arrows.front(), "book/up/" + std::to_string(level++)),
            ControlNode(arrows.front(), labels.front()));
    }
}

void AddSubheaderButtons(graph::GraphBuilder& builder, const std::string& key, const Widget* frame)
{
    const Widget* subheader = agui::member(frame, game::layout.frameSubheader);
    builder.BeginStop(key);
    builder.StartRow();
    int index = 0;
    for (const Widget* button : FindAll(subheader, "agui::Button"))
        AddControl(builder, key + "/" + std::to_string(index++), button);
    builder.EndRow();
}

void AddBlueprintList(graph::GraphBuilder& builder, const std::string& key, const Widget* list)
{
    if (!Shows(list))
        return;
    bool listView = agui::blueprintsListView(list);
    unsigned columns = listView ? 1 : agui::tableColumns(list);
    std::span<const Widget* const> cells = agui::children(list);
    if (columns == 0)
        return;
    int slotNumber = 0;
    for (std::size_t start = 0; start < cells.size(); start += columns)
    {
        bool rowStarted = false;
        for (std::size_t i = start; i < cells.size() && i < start + columns; ++i)
        {
            const Widget* slot = SlotIn(cells[i]);
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
                builder.StartRow(key);
                rowStarted = true;
            }
            builder.AddItem(graph::ControlId::Referenced(slot, key + "/" + std::to_string(slotNumber++)), std::move(node));
        }
        if (rowStarted)
            builder.EndRow();
    }
}

void AddListView(graph::GraphBuilder& builder, const Widget* frame)
{
    const Widget* subheader = agui::member(frame, game::layout.frameSubheader);
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

} // namespace fa::screens
