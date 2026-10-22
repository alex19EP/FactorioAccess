#include "EntityWindowScreen.hpp"

#include <algorithm>
#include <format>
#include <iterator>
#include <span>
#include <utility>

#include "GuiDump.hpp"
#include "game.h"
#include "parts.h"
#include "vocab.h"

namespace fa::screens
{

using agui::Widget;

namespace
{

// The interaction area inside remote view's frame. Searched by visibility alone: the frame and the
// area are no controls, so FindDescendant passes them by.
const Widget* InteractionArea(const Widget* widget)
{
    if (!agui::visible(widget))
        return nullptr;
    if (agui::derivesFrom(widget, "InteractionArea"))
        return widget;
    for (auto list : {agui::children(widget), agui::privateChildren(widget)})
        for (const Widget* child : list)
            if (const Widget* found = InteractionArea(child))
                return found;
    return nullptr;
}

// The application Gui's root while a game is played and the game menu is closed, which takes over.
const Widget* PlayRoot()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return nullptr;
    const agui::Gui* gui = agui::applicationGui();
    return gui ? agui::baseWidget(gui) : nullptr;
}

// Remote view's frame, in which it docks a window: a locomotive's always opens there, beside the
// train it follows, and the logistic networks window.
bool IsRemoteFrame(const Widget* widget) { return agui::derivesFrom(widget, "RemoteControllerView::FrameAround"); }

// The children of the interaction area of remote view's frame `frame`, while it shows.
std::span<const Widget* const> Docked(const Widget* frame)
{
    if (!agui::visible(frame))
        return {};
    const Widget* area = InteractionArea(frame);
    return area ? agui::children(area) : std::span<const Widget* const>{};
}

// Whether remote view docks a game window beside the map now, whichever screen reads it: each
// screen tells the map part the same.
bool DockedWindowShown()
{
    const Widget* root = PlayRoot();
    if (!root)
        return false;
    for (const Widget* child : agui::children(root))
        if (IsRemoteFrame(child))
            for (const Widget* docked : Docked(child))
                if (agui::visible(docked) && (agui::wrappedWindow(docked) || agui::derivesFrom(docked, "GameGui")))
                    return true;
    return false;
}

} // namespace

EntityWindowScreen::Found EntityWindowScreen::FindWindow() const
{
    const Widget* root = PlayRoot();
    if (!root)
        return {};
    Found top;
    auto consider = [&](const Widget* child, bool docked)
    {
        if (!agui::visible(child))
            return;
        if (const Widget* wrapped = agui::wrappedWindow(child))
        {
            if (Handles(wrapped))
                top = {wrapped, child, docked};
        }
        else if (Handles(child))
            top = {child, nullptr, docked};
    };
    for (const Widget* child : agui::children(root))
    {
        if (IsRemoteFrame(child))
        {
            for (const Widget* docked : Docked(child))
                consider(docked, true);
            continue;
        }
        consider(child, false);
    }
    return top;
}

bool EntityWindowScreen::IsActive()
{
    const bool docked = DockedWindowShown();
    parts::setAvailable(parts::Part::Map, docked);
    if (parts::current() == parts::Part::Map && !docked)
        parts::close();
    const Found found = FindWindow();
    const Widget* window = found.window;
    _docked = found.docked;
    // Steps aside while the player is on the map beside its window.
    if (!window || (found.docked && parts::current() == parts::Part::Map))
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
    ScopedBuildLander lander(
        [this](std::string key, std::function<bool()> ready) { LandWhen(std::move(key), std::move(ready)); });
    BuildWindow(builder, _window);
    if (found.wrapper)
        AddRelativeElements(builder, found.wrapper);
}

std::string EntityWindowScreen::LeaveLine() const
{
    return _docked && parts::current() == parts::Part::Map ? std::string(vocab::kMap) : std::string();
}

bool EntityWindowScreen::TypingIn(const graph::GraphNode& node) { return TypingInField(node); }

void EntityWindowScreen::OnCursorMoved(const graph::GraphNode& node) { FollowCursor(node); }

void EntityWindowScreen::OnPop()
{
    _window = nullptr;
    _landing.Reset();
}

void EntityWindowScreen::LandWhen(std::string key, std::function<bool()> ready)
{
    _landing.Set(std::move(key), std::move(ready));
}

const char* EntityWindowScreen::TakeSuggestedLanding()
{
    // Asked after Build, which returns early once the window is gone; nothing in it may be read then.
    if (!_window || FindWindow().window != _window)
    {
        _landing.Reset();
        return nullptr;
    }
    return _landing.Take();
}

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
