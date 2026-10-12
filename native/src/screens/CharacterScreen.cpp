#include "CharacterScreen.hpp"

#include <string>
#include <vector>

#include "AguiNodes.hpp"
#include "text.h"
#include "vocab.h"

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
    vtable.Announcements.emplace_back([list, recipe]()
        { return std::to_string(agui::recipeItem(list, recipe).craftable); }, true, graph::AnnouncementKinds::Value);
    return vtable;
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
    AddGrid(
        builder, key, agui::parent(slot), [](const Widget* cell)
        { return agui::derivesFrom(cell, "InventoryGuiSlot"); }, [](const Widget* cell) { return ControlNode(cell); });
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
    // The search button sits in the frame's title bar; it ends the row of tabs.
    const Widget* search = nullptr;
    for (const Widget* ancestor = agui::parent(crafting); ancestor && !search; ancestor = agui::parent(ancestor))
        if (agui::frameTitle(ancestor))
            search = FindDescendant(ancestor, "SearchBar");
    if (!groups.empty() || search)
    {
        builder.StartRow("groups");
        for (std::size_t i = 0; i < groups.size(); ++i)
            builder.AddItem(
                graph::ControlId::Referenced(groups[i], "groups/" + std::to_string(i)), ControlNode(groups[i]));
        if (search)
            builder.AddItem(graph::ControlId::Referenced(search, "groups/search"), ControlNode(search));
        builder.EndRow();
    }
    // The selected group's recipes: the only table of them the tabbed pane shows.
    if (const Widget* recipe = FindDescendant(crafting, "RecipeSlot"))
        AddGrid(
            builder, "recipes", agui::parent(recipe),
            [](const Widget* cell) { return agui::derivesFrom(cell, "RecipeSlot"); },
            [crafting](const Widget* slot) { return RecipeNode(crafting, slot); });

    if (!title.empty())
        builder.PopContext();
}

// The crafting queue under its label in the left pane, a row per row of the game's table; a slot
// is cancelled by clicking it, as at the bottom left.
void AddQueue(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* info = FindDescendant(window, "CharacterInfoGui");
    if (!info)
        return;
    agui::CharacterQueue queue = agui::characterQueue(info);
    if (!agui::visible(queue.label))
        return;
    builder.BeginStop("queue");
    std::string title = LabelText(queue.label);
    if (!title.empty())
        builder.PushContext(title);
    if (FindDescendant(queue.table, "CraftingQueueSlot"))
        AddGrid(
            builder, "queue", queue.table,
            [](const Widget* cell) { return agui::derivesFrom(cell, "CraftingQueueSlot"); },
            [](const Widget* cell) { return ControlNode(cell); });
    else
        builder.AddItem(graph::ControlId::Referenced(queue.table, "queue/empty"),
            TextNode(queue.table, []() { return std::string(vocab::kEmpty); }));
    if (!title.empty())
        builder.PopContext();
}

// The window's tabs (the map editor's Items, Infinity filters and Character), while it has more than
// one; a SelectListGui's item group tabs are its own.
void AddTabs(graph::GraphBuilder& builder, const Widget* window)
{
    std::vector<const Widget*> tabs;
    for (const Widget* tab : FindAll(window, "agui::Tab"))
        if (!agui::derivesFrom(tab, "ItemGroupTab"))
            tabs.push_back(tab);
    if (tabs.size() < 2)
        return;
    builder.BeginStop("tabs");
    builder.StartRow("tabs");
    for (std::size_t i = 0; i < tabs.size(); ++i)
        builder.AddItem(graph::ControlId::Referenced(tabs[i], "tabs/" + std::to_string(i)), ControlNode(tabs[i]));
    builder.EndRow();
}

// The map editor's item list (ItemSpawnGui, a SelectListGui of items), whose choice puts a stack of
// it in the hand.
void AddItemList(graph::GraphBuilder& builder, const Widget* widget)
{
    for (const Widget* child : VisibleChildren(widget))
    {
        if (!agui::derivesFromTemplate(child, "SelectListGui"))
        {
            AddItemList(builder, child);
            continue;
        }
        builder.BeginStop("items");
        std::string title = TitleAbove(child);
        if (!title.empty())
            builder.PushContext(title);
        AddChoices(builder, "items/", child);
        if (!title.empty())
            builder.PopContext();
        return;
    }
}

// The map editor's infinity filters: what the editor's inventory keeps topped up or empties.
void AddInfinityFilters(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* filters = FindDescendant(window, "InfinityEditorControllerFiltersGui");
    if (!filters)
        return;
    builder.BeginStop("filters");
    AddSubtree(builder, "filters", filters);
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
    AddTabs(builder, _window);
    if (const Widget* crafting = FindDescendant(_window, "CraftingGui"))
        AddCrafting(builder, crafting);
    else
        AddItemList(builder, _window);
    AddInfinityFilters(builder, _window);
    AddQueue(builder, _window);
}

const char* CharacterScreen::TakeSuggestedLanding()
{
    bool land = _landOnSearch;
    _landOnSearch = false;
    return land ? kSearchKey : nullptr;
}

bool CharacterScreen::TypingIn(const graph::GraphNode& node) { return TypingInField(node); }

void CharacterScreen::OnCursorMoved(const graph::GraphNode& node) { FollowCursor(node); }

void CharacterScreen::OnPop()
{
    _window = nullptr;
    _searching = false;
    _landOnSearch = false;
}

} // namespace fa::screens
