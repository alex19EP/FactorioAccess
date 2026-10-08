#include "MachineScreen.hpp"

#include <string>
#include <utility>
#include <vector>

#include "AguiNodes.hpp"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;

// Bars read with what they belong to rather than as stops of their own: the crafting progress and
// productivity with the first output slot (with the recipe while there is none); a drill's
// productivity with its mining progress; what is left of the burning fuel with the fuel slot.
// Enter on the recipe changes it where the machine allows. The slot tables, unnamed on screen,
// are named as the mod's entity menus named their inventories.
Attachments MachineAttachments(const agui::EntityWindowParts& parts)
{
    Attachments attachments;
    auto label = [&attachments](const Widget* container, std::string name)
    {
        if (container)
            attachments[container].label = name;
    };
    std::vector<std::pair<const Widget*, std::string>> bars;
    if (parts.progressBar && parts.recipe)
        bars.emplace_back(parts.progressBar, std::string(vocab::kProgress));
    if (parts.bonusBar)
        bars.emplace_back(parts.bonusBar, std::string(vocab::kProductivity));
    std::vector<const Widget*> outputs = parts.outputs ? VisibleChildren(parts.outputs) : std::vector<const Widget*>();
    if (parts.recipe)
    {
        attachments[parts.recipe].press = parts.changeRecipe;
        attachments[outputs.empty() ? parts.recipe : outputs.front()].bars = std::move(bars);
    }
    else if (parts.progressBar)
        attachments[parts.progressBar] = {std::move(bars), nullptr, std::string(vocab::kMining)};
    label(parts.inputs, vocab::kInputs);
    label(parts.outputs, vocab::kOutputs);
    label(parts.modules, vocab::kModules);
    for (const Widget* burner : FindAll(parts.entity, "BurnerInfo"))
    {
        agui::BurnerParts fuel = agui::burnerParts(burner);
        label(fuel.slots, vocab::kFuel);
        label(fuel.burntResults, vocab::kBurntResults);
        std::vector<const Widget*> slots = VisibleChildren(fuel.slots);
        if (!slots.empty())
            attachments[slots.front()].bars.emplace_back(fuel.bar, std::string(vocab::kBurning));
    }
    return attachments;
}

} // namespace

bool MachineScreen::Handles(const Widget* window) const
{
    // The planners' windows have recipes of their own.
    if (agui::derivesFrom(window, "DeconstructionItemGui") || agui::derivesFrom(window, "UpgradeItemGui"))
        return false;
    return agui::derivesFrom(window, "GameGuiWithControllerInventory")
        || agui::derivesFrom(window, "AssemblingMachineSelectRecipeGui");
}

void MachineScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    agui::EntityWindowParts parts = agui::entityWindowParts(window);
    if (!parts.entity)
    {
        // The recipe list: its group tabs, recipes, search and confirm, all in one stop.
        builder.BeginStop("window");
        AddSubtree(builder, "window", window);
        return;
    }
    agui::EntityPanelParts panel = agui::entityPanelParts(window);
    std::vector<const Widget*> skip;
    if (parts.inventoryPanel)
        skip.push_back(parts.inventoryPanel);
    if (panel.sidePanel)
        skip.push_back(panel.sidePanel);
    AddTitledWindow(builder, "entity", parts.entity, std::move(skip), MachineAttachments(parts));
    AddInventory(builder, parts);
    if (panel.sidePanel)
        AddSidePanel(builder, panel.sidePanel);
}

} // namespace fa::screens
