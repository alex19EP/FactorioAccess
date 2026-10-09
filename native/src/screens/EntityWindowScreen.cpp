#include "EntityWindowScreen.hpp"

#include <algorithm>
#include <format>
#include <iterator>
#include <utility>

#include "GuiDump.hpp"
#include "game.h"

namespace fa::screens
{

using agui::Widget;

EntityWindowScreen::Found EntityWindowScreen::FindWindow() const
{
    // Only over plain play: the game menu takes over.
    if (!agui::inGame() || agui::menuStateWindow())
        return {};
    const agui::Gui* gui = agui::applicationGui();
    const Widget* root = gui ? agui::baseWidget(gui) : nullptr;
    if (!root)
        return {};
    Found top;
    for (const Widget* child : agui::children(root))
    {
        if (!agui::visible(child))
            continue;
        if (const Widget* wrapped = agui::wrappedWindow(child))
        {
            if (Handles(wrapped))
                top = {wrapped, child};
        }
        else if (Handles(child))
            top = {child, nullptr};
    }
    return top;
}

bool EntityWindowScreen::IsActive()
{
    const Widget* window = FindWindow().window;
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

void EntityWindowScreen::Build(graph::GraphBuilder& builder)
{
    // A click can close the window or replace it, so it is looked up again before anything in it
    // is read.
    Found found = FindWindow();
    if (!_window || found.window != _window)
        return;
    DumpWindow(_window, _class);
    BuildWindow(builder, _window);
    if (found.wrapper)
        AddRelativeElements(builder, found.wrapper);
}

bool EntityWindowScreen::TypingIn(const graph::GraphNode& node) { return TypingInField(node); }

void EntityWindowScreen::OnCursorMoved(const graph::GraphNode& node) { FollowCursor(node); }

void EntityWindowScreen::OnPop() { _window = nullptr; }

void EntityWindowScreen::AddTitledWindow(graph::GraphBuilder& builder, const std::string& key, const Widget* window,
    std::vector<const Widget*> skip, Attachments attachments)
{
    builder.BeginStop(key);
    const Widget* header = agui::member(window, game::layout.frameHeader);
    skip.push_back(header);
    // The title bar after the content, so the stop opens on the entity's status.
    const Widget* title = agui::frameTitle(window);
    std::vector<const Widget*> headerSkip{title};
    std::ranges::copy(FindAll(header, "CloseButton"), std::back_inserter(headerSkip));
    std::ranges::copy(FindAll(header, "SearchBar"), std::back_inserter(headerSkip));

    std::string titleText = title && Shows(title) ? LabelText(title) : std::string();
    if (!titleText.empty())
        builder.PushContext(titleText);
    AddSubtree(builder, key, window, std::move(skip), std::move(attachments));
    AddSubtree(builder, key + "/header", header, std::move(headerSkip));
    if (!titleText.empty())
        builder.PopContext();
}

void EntityWindowScreen::AddInventory(graph::GraphBuilder& builder, const agui::EntityWindowParts& parts)
{
    if (parts.ghostChoices && Shows(parts.ghostChoices))
    {
        // Titled in its subheader ("Ghost cursor selection"), read as the context of its choices.
        builder.BeginStop("inventory");
        std::vector<const Widget*> labels =
            FindAll(agui::member(parts.ghostChoices, game::layout.itemSelectListSubheader), "agui::Label");
        std::string title = labels.empty() ? std::string() : LabelText(labels.front());
        if (!title.empty())
            builder.PushContext(title);
        AddChoices(builder, "inventory/", parts.ghostChoices);
        if (!title.empty())
            builder.PopContext();
        return;
    }
    if (!parts.inventory || !Shows(parts.inventory))
        return;
    builder.BeginStop("inventory");
    std::string title =
        parts.inventoryTitle && Shows(parts.inventoryTitle) ? LabelText(parts.inventoryTitle) : std::string();
    if (!title.empty())
        builder.PushContext(title);
    AddSubtree(builder, "inventory", parts.inventory);
    if (!title.empty())
        builder.PopContext();
}

void EntityWindowScreen::AddSidePanel(graph::GraphBuilder& builder, const Widget* sidePanel)
{
    if (!Shows(sidePanel) || !HasContent(sidePanel))
        return;
    builder.BeginStop("panel");
    AddSubtree(builder, "panel", sidePanel);
}

void EntityWindowScreen::AddRelativeElements(graph::GraphBuilder& builder, const Widget* wrapper)
{
    std::size_t index = 0;
    for (const Widget* flow : agui::relativeFlows(wrapper))
        for (const Widget* element : VisibleChildren(flow))
        {
            if (!HasContent(element))
                continue;
            std::string key = std::format("relative/{}", index++);
            builder.BeginStop(key);
            AddSubtree(builder, key, element);
        }
}

} // namespace fa::screens
