#include "BlueprintLibraryScreen.hpp"

#include <string>
#include <vector>

#include "BlueprintLists.hpp"
#include "game.h"

namespace fa::screens
{

namespace
{

using agui::Widget;
using game::layout;

// The title bar's back and forward arrows through the window's history, and the warning that the
// library takes a lot of memory.
void AddHistory(graph::GraphBuilder& builder, const Widget* window, const agui::EntityWindowParts& parts)
{
    builder.BeginStop("history");
    builder.StartRow();
    int index = 0;
    for (const Widget* arrow : FindAll(parts.header, "BrowseArrow"))
        AddControl(builder, "history/" + std::to_string(index++), arrow);
    builder.EndRow();
    const Widget* memory = agui::member(window, layout.libraryMemory);
    if (Shows(memory) && !LabelText(memory).empty())
        builder.AddItem(graph::ControlId::Referenced(memory, "history/memory"),
            TextNode(memory, [memory]() { return LabelText(memory); }));
}

// The tabs, then the shelves of the chosen one: the "synchronising" label until a shelf has
// arrived, then its records.
void AddShelves(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* tabs = agui::member(window, layout.libraryTabs);
    builder.BeginStop("tabs");
    builder.StartRow();
    int index = 0;
    for (const Widget* tab : FindAll(tabs, "agui::Tab"))
        AddControl(builder, "tabs/" + std::to_string(index++), tab);
    builder.EndRow();

    builder.BeginStop("shelves");
    index = 0;
    for (const Widget* shelf : FindAll(tabs, "BlueprintShelfWidget"))
    {
        std::string key = "shelves/" + std::to_string(index++);
        // The shelf holds one of the two at a time; the other stays a member, out of the tree.
        const Widget* synchronising = agui::member(shelf, layout.shelfSynchronising);
        const Widget* list = agui::member(shelf, layout.shelfList);
        if (agui::parent(synchronising) == shelf)
            builder.AddItem(graph::ControlId::Referenced(synchronising, key + "/synchronising"),
                TextNode(synchronising, [synchronising]() { return LabelText(synchronising); }));
        else if (agui::parent(list) == shelf)
            AddBlueprintList(builder, key, list);
    }
}

// A book record opened in the library, laid out as a book's window.
void AddOpenBook(graph::GraphBuilder& builder, const Widget* book)
{
    const Widget* header = agui::member(book, layout.bookRecordGuiNavigation);
    AddBook(builder,
        {agui::member(header, layout.bookHeaderName),
            agui::member(header, layout.bookHeaderRename),
            agui::member(book, layout.bookRecordGuiDescription),
            header});
    AddSubheaderButtons(builder, "buttons", agui::member(book, layout.bookRecordGuiHeader));
    builder.BeginStop("contents");
    AddBlueprintList(builder, "contents", agui::member(book, layout.bookRecordGuiList));
    AddListView(builder, agui::member(book, layout.bookRecordGuiInside));
}

} // namespace

bool BlueprintLibraryScreen::Handles(const Widget* window) const
{
    return agui::derivesFrom(window, "BlueprintLibraryGui");
}

void BlueprintLibraryScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    agui::EntityWindowParts parts = agui::entityWindowParts(window);
    const Widget* inside = agui::member(window, layout.libraryInside);
    if (Shows(inside))
    {
        AddShelves(builder, window);
        AddListView(builder, inside);
    }
    else if (const Widget* book = FindDescendant(agui::member(window, layout.libraryBookHolder), "BlueprintBookRecordWidget"))
        AddOpenBook(builder, book);
    // After the panel: when a book opens or closes, the cursor finds no earlier stop that stayed
    // and starts at the top of the new panel.
    AddHistory(builder, window, parts);
    AddInventory(builder, parts);
}

} // namespace fa::screens
