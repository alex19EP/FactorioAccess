#include "DeconstructionPlannerScreen.hpp"

#include <string>

#include "AguiNodes.hpp"
#include "game.h"

namespace fa::screens
{

namespace
{

using agui::Widget;
using game::layout;

// The planner's name, then every button for it in the order the game shows them.
void AddPlanner(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* part = agui::member(window, layout.deconItemPart);
    const Widget* name = agui::member(part, layout.deconItemPartName);
    builder.BeginStop("planner");
    builder.StartRow();
    if (Shows(name))
        builder.AddItem(graph::ControlId::Referenced(name, "planner/name"),
            TextNode(name, [name]() { return LabelText(name); }));
    int index = 0;
    for (const Widget* button : FindAll(agui::member(part, layout.frameSubheader), "agui::Button"))
        AddControl(builder, "planner/" + std::to_string(index++), button);
    builder.EndRow();
    const Widget* description = agui::member(window, layout.deconDescription);
    if (Shows(description) && !LabelText(description).empty())
        builder.AddItem(graph::ControlId::Referenced(description, "planner/description"),
            TextNode(description, [description]() { return LabelText(description); }));
}

} // namespace

bool DeconstructionPlannerScreen::Handles(const Widget* window) const
{
    return agui::derivesFrom(window, "DeconstructionItemGui");
}

void DeconstructionPlannerScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    agui::EntityWindowParts parts = agui::entityWindowParts(window);
    const Widget* title = agui::frameTitle(parts.entity);
    std::string titleText = title && Shows(title) ? LabelText(title) : std::string();

    AddPlanner(builder, window);

    // The tab that is not chosen keeps its content, out of the window.
    const bool tiles = agui::tabSelected(agui::member(window, layout.deconTileTab));
    builder.BeginStop("settings");
    if (!titleText.empty())
        builder.PushContext(titleText);
    AddControl(builder, "settings/trees", agui::member(window, layout.deconTreesAndRocks));
    builder.StartRow();
    AddControl(builder, "settings/entity-tab", agui::member(window, layout.deconEntityTab));
    AddControl(builder, "settings/tile-tab", agui::member(window, layout.deconTileTab));
    builder.EndRow();
    const Widget* mode = agui::member(window, tiles ? layout.deconTileMode : layout.deconEntityMode);
    if (Shows(mode))
        builder.AddItem(graph::ControlId::Referenced(mode, "settings/mode"), LabeledSwitchNode(mode));
    // The tile mode has no label on screen; its tooltip explains each choice.
    if (tiles)
        AddControl(builder, "settings/tile-mode", agui::member(window, layout.deconTileSelection),
            []() { return std::string(); });
    if (!titleText.empty())
        builder.PopContext();

    // Keyed by tab, so each tab's slots keep their own position.
    const std::string filters = tiles ? "tile-filters" : "entity-filters";
    builder.BeginStop(filters);
    if (!titleText.empty())
        builder.PushContext(titleText);
    AddGrid(builder, filters, agui::member(window, tiles ? layout.deconTileFilters : layout.deconEntityFilters),
        [](const Widget* cell) { return agui::derivesFrom(cell, "ChooseButtonBase"); },
        [](const Widget* cell) { return ControlNode(cell); });
    if (!titleText.empty())
        builder.PopContext();

    AddInventory(builder, parts);
}

} // namespace fa::screens
