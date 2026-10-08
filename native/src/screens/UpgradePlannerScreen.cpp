#include "UpgradePlannerScreen.hpp"

#include <format>
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

// The planner's name, then every button for it in the order the game shows them.
void AddPlanner(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* frame = agui::member(window, layout.upgradeItemFrame);
    const Widget* name = agui::member(frame, layout.upgradeItemName);
    builder.BeginStop("planner");
    builder.StartRow();
    if (Shows(name))
        builder.AddItem(graph::ControlId::Referenced(name, "planner/name"),
            TextNode(name, [name]() { return LabelText(name); }));
    int index = 0;
    for (const Widget* button : FindAll(agui::member(frame, layout.frameSubheader), "agui::Button"))
        AddControl(builder, "planner/" + std::to_string(index++), button);
    builder.EndRow();
    const Widget* description = agui::member(window, layout.upgradeDescription);
    if (Shows(description) && !LabelText(description).empty())
        builder.AddItem(graph::ControlId::Referenced(description, "planner/description"),
            TextNode(description, [description]() { return LabelText(description); }));
}

// A row per rule in a context of its number. The table's cells are the header pairs first, then a
// flow per rule holding its From and To slots, in the planner's order.
void AddRules(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* table = agui::member(window, layout.upgradeRules);
    if (!Shows(table))
        return;
    const Widget* fromHeader = nullptr;
    const Widget* toHeader = nullptr;
    int rule = 0;
    for (const Widget* cell : agui::children(table))
    {
        if (!agui::visible(cell))
            continue;
        std::vector<const Widget*> slots = FindAll(cell, "ChooseButtonBase");
        if (slots.size() != 2)
        {
            std::vector<const Widget*> labels = FindAll(cell, "agui::Label");
            if (!fromHeader && labels.size() == 2)
            {
                fromHeader = labels[0];
                toHeader = labels[1];
            }
            continue;
        }
        ++rule;
        builder.PushContext(std::to_string(rule), "", /*positions*/ false);
        builder.StartRow("rule");
        const char* sides[] = {"from", "to"};
        const Widget* headers[] = {fromHeader, toHeader};
        for (int side = 0; side < 2; ++side)
        {
            graph::NodeVtable node = headers[side] ? ControlNode(slots[side], headers[side]) : ControlNode(slots[side]);
            // The header already says which side it is; "1 of 2" would only repeat it.
            node.SpeaksOwnPosition = true;
            builder.AddItem(
                graph::ControlId::Referenced(slots[side], std::format("rules/{}/{}", rule, sides[side])), std::move(node));
        }
        builder.EndRow();
        builder.PopContext();
    }
}

} // namespace

bool UpgradePlannerScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "UpgradeItemGui"); }

void UpgradePlannerScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    agui::EntityWindowParts parts = agui::entityWindowParts(window);
    const Widget* title = agui::frameTitle(parts.entity);
    std::string titleText = title && Shows(title) ? LabelText(title) : std::string();

    AddPlanner(builder, window);

    builder.BeginStop("rules");
    if (!titleText.empty())
        builder.PushContext(titleText);
    AddRules(builder, window);
    if (!titleText.empty())
        builder.PopContext();

    AddInventory(builder, parts);
}

} // namespace fa::screens
