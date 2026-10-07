#include "CharacterScreen.hpp"

#include <functional>
#include <string>
#include <vector>

#include "AguiNodes.hpp"
#include "text.h"

namespace fa::screens
{

namespace
{

using agui::Widget;

constexpr const char* kSearchKey = "search/field";

const Widget* FindWindow()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return nullptr;
    const agui::Gui* gui = agui::applicationGui();
    const Widget* root = gui ? agui::baseWidget(gui) : nullptr;
    if (!root)
        return nullptr;
    for (const Widget* child : agui::children(root))
        if (agui::visible(child) && agui::derivesFrom(child, "ControllerGui"))
            return child;
    return nullptr;
}

// The caption of the nearest titled frame around `widget`: "Character", "Crafting".
std::string TitleAbove(const Widget* widget)
{
    for (const Widget* ancestor = agui::parent(widget); ancestor; ancestor = agui::parent(ancestor))
        if (const Widget* title = agui::frameTitle(ancestor))
            if (std::string text = text::speakable(agui::text(title)); !text.empty())
                return text;
    return {};
}

// "iron gear wheel, 12": the recipe and how many the player can craft now. Clicked, and its tooltip
// read, like any slot.
graph::NodeVtable RecipeNode(const Widget* list, const Widget* recipe)
{
    graph::NodeVtable vtable = ControlNode(recipe);
    vtable.Announcements.clear();
    vtable.Announcements.emplace_back([list, recipe]() { return std::string(agui::recipeItem(list, recipe).name); },
        false, graph::AnnouncementKinds::Label);
    vtable.Announcements.emplace_back(
        [list, recipe]() { return std::to_string(agui::recipeItem(list, recipe).craftable); }, true,
        graph::AnnouncementKinds::Value);
    return vtable;
}

// A table of buttons as the game lays it out: a row of the grid per table row, keeping the column
// on Up and Down. Cells that are not `className` (the fillers that end a subgroup's line) are
// skipped, and a row of fillers alone is left out.
void AddGrid(graph::GraphBuilder& builder, const std::string& prefix, const Widget* table, std::string_view className,
    const std::function<graph::NodeVtable(const Widget*)>& node)
{
    unsigned columns = agui::tableColumns(table);
    auto cells = agui::children(table);
    if (columns == 0)
        return;
    for (std::size_t start = 0; start < cells.size(); start += columns)
    {
        std::vector<std::size_t> row;
        for (std::size_t i = start; i < cells.size() && i < start + columns; ++i)
            if (agui::visible(cells[i]) && agui::derivesFrom(cells[i], className))
                row.push_back(i);
        if (row.empty())
            continue;
        builder.StartRow(prefix);
        for (std::size_t i : row)
            builder.AddItem(graph::ControlId::Referenced(cells[i], prefix + "/" + std::to_string(i)), node(cells[i]));
        builder.EndRow();
    }
}

void AddInventory(graph::GraphBuilder& builder, const std::string& key, const Widget* inventory)
{
    const Widget* slot = FindDescendant(inventory, "InventoryGuiSlot");
    if (!slot)
        return;
    builder.BeginStop(key);
    std::string title = TitleAbove(inventory);
    if (!title.empty())
        builder.PushContext(title);
    AddGrid(builder, key, agui::parent(slot), "InventoryGuiSlot", [](const Widget* cell) { return ControlNode(cell); });
    if (!title.empty())
        builder.PopContext();
}

void AddCrafting(graph::GraphBuilder& builder, const Widget* crafting)
{
    builder.BeginStop("crafting");
    std::string title = TitleAbove(crafting);
    if (!title.empty())
        builder.PushContext(title);

    std::vector<const Widget*> groups = FindAll(crafting, "ItemGroupTab");
    if (!groups.empty())
    {
        builder.StartRow("groups");
        for (std::size_t i = 0; i < groups.size(); ++i)
            builder.AddItem(graph::ControlId::Referenced(groups[i], "groups/" + std::to_string(i)), ControlNode(groups[i]));
        builder.EndRow();
    }
    // The selected group's recipes: the only table of them the tabbed pane shows.
    if (const Widget* recipe = FindDescendant(crafting, "RecipeSlot"))
        AddGrid(builder, "recipes", agui::parent(recipe), "RecipeSlot",
            [crafting](const Widget* slot) { return RecipeNode(crafting, slot); });

    if (!title.empty())
        builder.PopContext();
}

// The search field the game's focus-search control (Ctrl+F) opens over the window, while it shows.
const Widget* SearchField(const Widget* window)
{
    const Widget* popup = FindDescendant(window, "SearchPopup");
    return popup ? FindDescendant(popup, "agui::TextField") : nullptr;
}

} // namespace

bool CharacterScreen::IsActive()
{
    const Widget* window = FindWindow();
    if (!window || (_window && window != _window))
    {
        // Gone, or a new window in its place: one inactive frame pops this screen.
        _window = nullptr;
        return false;
    }
    _window = window;
    return true;
}

void CharacterScreen::Build(graph::GraphBuilder& builder)
{
    if (!_window || FindWindow() != _window)
        return;
    // Searching filters the recipes as it is typed; opening it lands on the field.
    const Widget* search = SearchField(_window);
    if (search)
    {
        builder.BeginStop("search");
        builder.AddItem(graph::ControlId::Referenced(search, kSearchKey), ControlNode(search));
    }
    _landOnSearch = search && !_searching;
    _searching = search != nullptr;
    std::vector<const Widget*> inventories = FindAll(_window, "InventoryGui");
    for (std::size_t i = 0; i < inventories.size(); ++i)
        AddInventory(builder, "inventory" + std::to_string(i), inventories[i]);
    if (const Widget* crafting = FindDescendant(_window, "CraftingGui"))
        AddCrafting(builder, crafting);
}

const char* CharacterScreen::TakeSuggestedLanding()
{
    bool land = _landOnSearch;
    _landOnSearch = false;
    return land ? kSearchKey : nullptr;
}

bool CharacterScreen::TypingIn(const graph::GraphNode& node)
{
    // The retained render may be frames old, so the widget is compared before it is read: when it
    // is the game's focused widget, it is alive.
    const agui::Gui* gui = agui::applicationGui();
    const Widget* focused = gui ? agui::focusedWidget(gui) : nullptr;
    return focused && focused == node.Vtable.HostTag && agui::kind(focused) == agui::Kind::TextBox
        && !agui::readOnly(focused);
}

void CharacterScreen::OnCursorMoved(const graph::GraphNode& node)
{
    auto* widget = static_cast<const Widget*>(node.Vtable.HostTag);
    if (!widget)
        return;
    agui::scrollIntoView(widget);
    // Back on the search field, typing goes to it again.
    if (agui::kind(widget) == agui::Kind::TextBox && agui::isFocusable(widget))
        agui::focus(widget);
}

void CharacterScreen::OnPop()
{
    _window = nullptr;
    _searching = false;
    _landOnSearch = false;
}

} // namespace fa::screens
