#include "BlueprintLibraryScreen.hpp"

#include <string>
#include <vector>

#include "BlueprintLists.hpp"
#include "game.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;
using game::layout;

constexpr const char* kSearchKey = "search/field";

// The title bar's search: its button, then its field while the search is open, named by the button
// ("Search (Ctrl + F)"). The field filters every shelf and an open book's contents as it is typed
// in, and the records follow it, so Down goes from the field to the first match.
void AddSearch(graph::GraphBuilder& builder, const Widget* header, const Widget* field)
{
    const Widget* button = FindDescendant(header, "SearchBar");
    AddControl(builder, "search/button", button);
    if (field)
        builder.AddItem(graph::ControlId::Referenced(field, kSearchKey),
            ControlNode(field, [button]() { return button ? NameOf(button) : std::string(); }));
}

// While searching, a list the search emptied says so.
void AddNoMatch(graph::GraphBuilder& builder, const Widget* field, int records)
{
    if (field && records == 0)
        builder.AddLabel(graph::ControlId::Structural("search/none"), []() { return std::string(vocab::kEmpty); });
}

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

// The tabs, then the search and the shelves of the chosen one: the "synchronising" label until a
// shelf has arrived, then its records.
void AddShelves(graph::GraphBuilder& builder, const Widget* window, const Widget* header, const Widget* field)
{
    const Widget* tabs = agui::member(window, layout.libraryTabs);
    builder.BeginStop("tabs");
    builder.StartRow();
    int index = 0;
    for (const Widget* tab : FindAll(tabs, "agui::Tab"))
        AddControl(builder, "tabs/" + std::to_string(index++), tab);
    builder.EndRow();

    builder.BeginStop("shelves");
    AddSearch(builder, header, field);
    index = 0;
    int records = 0;
    for (const Widget* shelf : FindAll(tabs, "BlueprintShelfWidget"))
    {
        std::string key = "shelves/" + std::to_string(index++);
        // The shelf holds one of the two at a time; the other stays a member, out of the tree.
        const Widget* synchronising = agui::member(shelf, layout.shelfSynchronising);
        const Widget* list = agui::member(shelf, layout.shelfList);
        if (agui::parent(synchronising) == shelf)
        {
            builder.AddItem(graph::ControlId::Referenced(synchronising, key + "/synchronising"),
                TextNode(synchronising, [synchronising]() { return LabelText(synchronising); }));
            ++records;
        }
        else if (agui::parent(list) == shelf)
            records += AddBlueprintList(builder, key, list);
    }
    AddNoMatch(builder, field, records);
}

// A book record opened in the library, laid out as a book's window, with the search over its
// contents.
void AddOpenBook(graph::GraphBuilder& builder, const Widget* book, const Widget* header, const Widget* field)
{
    const Widget* navigation = agui::member(book, layout.bookRecordGuiNavigation);
    AddBook(builder,
        {agui::member(navigation, layout.bookHeaderName),
            agui::member(navigation, layout.bookHeaderRename),
            agui::member(book, layout.bookRecordGuiDescription),
            navigation});
    AddSubheaderButtons(builder, "buttons", agui::member(book, layout.bookRecordGuiHeader));
    builder.BeginStop("contents");
    AddSearch(builder, header, field);
    AddNoMatch(builder, field, AddBlueprintList(builder, "contents", agui::member(book, layout.bookRecordGuiList)));
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
    // The search field shows only while the search is open, in a popup the game puts over the
    // window; opening it lands on it.
    const Widget* popup = FindDescendant(window, "SearchPopup");
    const Widget* field = popup ? FindDescendant(popup, "agui::TextField") : nullptr;
    if (field && !_searching)
        _landing = kSearchKey;
    _searching = field != nullptr;

    const Widget* inside = agui::member(window, layout.libraryInside);
    if (Shows(inside))
    {
        AddShelves(builder, window, parts.header, field);
        AddListView(builder, inside);
    }
    else if (const Widget* book = FindDescendant(agui::member(window, layout.libraryBookHolder), "BlueprintBookRecordWidget"))
        AddOpenBook(builder, book, parts.header, field);
    // After the panel: when a book opens or closes, the cursor finds no earlier stop that stayed
    // and starts at the top of the new panel.
    AddHistory(builder, window, parts);
    AddInventory(builder, parts);
}

const char* BlueprintLibraryScreen::TakeSuggestedLanding()
{
    const char* landing = _landing;
    _landing = nullptr;
    return landing;
}

void BlueprintLibraryScreen::OnPop()
{
    EntityWindowScreen::OnPop();
    _searching = false;
    _landing = nullptr;
}

} // namespace fa::screens
