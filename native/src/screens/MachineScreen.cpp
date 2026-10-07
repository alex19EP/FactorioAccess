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
    auto label = [&attachments](const Widget* container, std::string_view name)
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

void AddInventory(graph::GraphBuilder& builder, const agui::EntityWindowParts& parts)
{
    if (!parts.inventory || !Shows(parts.inventory))
        return;
    builder.BeginStop("inventory");
    std::string title = parts.inventoryTitle && Shows(parts.inventoryTitle) ? LabelText(parts.inventoryTitle)
                                                                            : std::string();
    if (!title.empty())
        builder.PushContext(title);
    AddSubtree(builder, "inventory", parts.inventory);
    if (!title.empty())
        builder.PopContext();
}

} // namespace

bool MachineScreen::Handles(const Widget* window) const
{
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
    std::vector<const Widget*> skip;
    if (parts.inventoryPanel)
        skip.push_back(parts.inventoryPanel);
    AddTitledWindow(builder, "entity", parts.entity, std::move(skip), MachineAttachments(parts));
    AddInventory(builder, parts);
}

} // namespace fa::screens
