#include "MachineScreen.hpp"

#include <algorithm>
#include <ranges>
#include <unordered_map>
#include <vector>

#include "AguiNodes.hpp"
#include "GuiDump.hpp"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Widget;

bool IsMachineWindow(const Widget* window)
{
    return agui::derivesFrom(window, "GameGuiWithControllerInventory")
        || agui::derivesFrom(window, "AssemblingMachineSelectRecipeGui");
}

// The topmost open entity window, or null. Only over plain play: the game menu takes over.
const Widget* FindWindow()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return nullptr;
    const agui::Gui* gui = agui::applicationGui();
    const Widget* root = gui ? agui::baseWidget(gui) : nullptr;
    if (!root)
        return nullptr;
    const Widget* top = nullptr;
    for (const Widget* child : agui::children(root))
        if (agui::visible(child) && IsMachineWindow(child))
            top = child;
    return top;
}

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

void AddEntity(graph::GraphBuilder& builder, const agui::EntityWindowParts& parts)
{
    builder.BeginStop("entity");
    std::vector<const Widget*> skip{parts.header};
    if (parts.inventoryPanel)
        skip.push_back(parts.inventoryPanel);
    // The title bar after the content, so the stop opens on the entity's status: its circuit and
    // logistic network buttons, without the title (the context already), close (Escape) or search.
    const Widget* title = agui::frameTitle(parts.entity);
    std::vector<const Widget*> headerSkip{title};
    std::ranges::copy(FindAll(parts.header, "CloseButton"), std::back_inserter(headerSkip));
    std::ranges::copy(FindAll(parts.header, "SearchBar"), std::back_inserter(headerSkip));

    std::string titleText = title && Shows(title) ? LabelText(title) : std::string();
    if (!titleText.empty())
        builder.PushContext(titleText);
    AddSubtree(builder, "entity", parts.entity, std::move(skip), MachineAttachments(parts));
    AddSubtree(builder, "header", parts.header, std::move(headerSkip));
    if (!titleText.empty())
        builder.PopContext();
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

bool MachineScreen::IsActive()
{
    const Widget* window = FindWindow();
    if (!window)
    {
        _window = nullptr;
        return false;
    }
    // Keyed by class too, not address alone: the game reuses a closed window's memory.
    const std::string& cls = agui::className(window);
    if (_window && (window != _window || cls != _class))
    {
        // A different window: one inactive frame pops this screen, so the next one starts fresh.
        _window = nullptr;
        return false;
    }
    _window = window;
    _class = cls;
    return true;
}

void MachineScreen::Build(graph::GraphBuilder& builder)
{
    // A click can close the window or replace it (choosing a recipe opens the assembler's own
    // window), so it is looked up again before anything in it is read.
    if (!_window || FindWindow() != _window)
        return;
    DumpWindow(_window, _class);
    agui::EntityWindowParts parts = agui::entityWindowParts(_window);
    if (!parts.entity)
    {
        // The recipe list: its group tabs, recipes, search and confirm, all in one stop.
        builder.BeginStop("window");
        AddSubtree(builder, "window", _window);
        return;
    }
    AddEntity(builder, parts);
    AddInventory(builder, parts);
}

bool MachineScreen::TypingIn(const graph::GraphNode& node) { return TypingInField(node); }

void MachineScreen::OnCursorMoved(const graph::GraphNode& node) { FollowCursor(node); }

void MachineScreen::OnPop()
{
    _window = nullptr;
}

} // namespace fa::screens
