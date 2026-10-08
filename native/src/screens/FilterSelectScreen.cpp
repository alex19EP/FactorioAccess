#include "FilterSelectScreen.hpp"

#include <format>
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
    // Every chooser is a SelectListGui<T>; the assembler's recipe list is one too, read with its
    // machine's window.
    for (const Widget* child : agui::children(root))
        if (agui::visible(child) && agui::derivesFromTemplate(child, "SelectListGui")
            && !agui::derivesFrom(child, "AssemblingMachineSelectRecipeGui"))
            return child;
    return nullptr;
}

// A choice by its name alone ("rare iron plate"): what it would make the filter, not a count.
graph::NodeVtable ChoiceNode(const Widget* choice)
{
    graph::NodeVtable vtable = ControlNode(choice);
    if (!agui::isSlotButton(choice))
        return vtable;
    vtable.Announcements.clear();
    vtable.Announcements.emplace_back(
        [choice]()
        {
            agui::SlotButton button = agui::slotButton(choice);
            return button.quality.empty() ? std::string(button.name)
                                          : std::format("{} {}", button.quality, button.name);
        },
        false, graph::AnnouncementKinds::Label);
    return vtable;
}

const Widget* SearchField(const Widget* window)
{
    const Widget* popup = FindDescendant(window, "SearchPopup");
    return popup ? FindDescendant(popup, "agui::TextField") : nullptr;
}

} // namespace

bool FilterSelectScreen::IsActive()
{
    const Widget* window = FindWindow();
    if (!window || (_window && window != _window))
    {
        // Gone, or a new chooser in its place: one inactive frame pops this screen.
        _window = nullptr;
        return false;
    }
    _window = window;
    return true;
}

void FilterSelectScreen::Build(graph::GraphBuilder& builder)
{
    if (!_window || FindWindow() != _window)
        return;
    const Widget* search = SearchField(_window);
    if (search)
    {
        builder.BeginStop("search");
        builder.AddItem(graph::ControlId::Referenced(search, kSearchKey), ControlNode(search));
    }
    _landOnSearch = search && !_searching;
    _searching = search != nullptr;

    builder.BeginStop("choices");
    std::string title;
    if (const Widget* label = agui::frameTitle(_window))
        title = text::speakable(agui::text(label));
    if (!title.empty())
        builder.PushContext(title);

    std::vector<const Widget*> groups = FindAll(_window, "ItemGroupTab");
    const Widget* searchButton = FindDescendant(_window, "SearchBar");
    if (!groups.empty() || searchButton)
    {
        builder.StartRow("groups");
        for (std::size_t i = 0; i < groups.size(); ++i)
            builder.AddItem(graph::ControlId::Referenced(groups[i], "groups/" + std::to_string(i)), ControlNode(groups[i]));
        if (searchButton)
            builder.AddItem(graph::ControlId::Referenced(searchButton, "groups/search"), ControlNode(searchButton));
        builder.EndRow();
    }
    // The selected group's choices: the table of slot buttons, with fillers ending each subgroup.
    for (const Widget* table : FindAll(_window, "agui::Table"))
    {
        auto cells = agui::children(table);
        bool choices = false;
        for (const Widget* cell : cells)
            choices |= agui::isSlotButton(cell);
        if (!choices)
            continue;
        AddGrid(
            builder, "choices", table, [](const Widget* cell) { return agui::isSlotButton(cell); },
            [](const Widget* cell) { return ChoiceNode(cell); });
    }

    if (!title.empty())
        builder.PopContext();
}

const char* FilterSelectScreen::TakeSuggestedLanding()
{
    bool land = _landOnSearch;
    _landOnSearch = false;
    return land ? kSearchKey : nullptr;
}

bool FilterSelectScreen::TypingIn(const graph::GraphNode& node) { return TypingInField(node); }

void FilterSelectScreen::OnCursorMoved(const graph::GraphNode& node) { FollowCursor(node); }

void FilterSelectScreen::OnPop()
{
    _window = nullptr;
    _searching = false;
    _landOnSearch = false;
}

} // namespace fa::screens
