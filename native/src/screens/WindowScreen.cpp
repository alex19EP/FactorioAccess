#include "WindowScreen.hpp"

#include <algorithm>
#include <vector>

#include "AguiNodes.hpp"
#include "GuiDump.hpp"
#include "text.h"

namespace fa::screens
{

const agui::Gui* WindowScreen::s_gui = nullptr;

const agui::Widget* WindowScreen::TopWindow()
{
    if (!s_gui || s_gui != agui::applicationGui())
        return nullptr;
    const agui::Widget* root = agui::baseWidget(s_gui);
    if (!root)
        return nullptr;
    // Over a loaded game, only while a menu is open: the game's own windows belong to the mod.
    const agui::Widget* menu = nullptr;
    if (agui::inGame())
    {
        menu = agui::menuStateWindow();
        if (!menu)
            return nullptr;
    }

    // An open modal owns the keyboard in the game too; only its window is reachable.
    if (const agui::Widget* modal = agui::topModal(s_gui))
    {
        const agui::Widget* top = modal;
        while (agui::parent(top) && agui::parent(top) != root)
            top = agui::parent(top);
        if (agui::parent(top) == root && agui::visible(top) && agui::kind(top) == agui::Kind::Window)
            return top;
    }
    if (menu)
        return agui::visible(menu) ? menu : nullptr;
    // Otherwise the last visible window, the one drawn on top. An empty one (the game keeps a bare
    // window over the main menu for a while after startup) has nothing to read and is passed over.
    const agui::Widget* top = nullptr;
    for (const agui::Widget* child : agui::children(root))
        if (agui::visible(child) && agui::kind(child) == agui::Kind::Window && HasContent(child))
            top = child;
    return top;
}

bool WindowScreen::IsActive()
{
    const agui::Widget* window = TopWindow();
    if (!window || !Handles(window))
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

void WindowScreen::Build(graph::GraphBuilder& builder)
{
    // A press can close the window between IsActive and this build (Mod settings in the map
    // generator does), so it is looked up again from the root before anything in it is read.
    if (!_window || TopWindow() != _window)
        return;
    DumpWindow(_window, _class);
    const agui::Widget* title = agui::frameTitle(_window);
    std::string titleText = title ? text::speakable(agui::text(title)) : std::string();
    if (!titleText.empty())
        builder.PushContext(titleText);
    BuildWindow(builder, _window);
    if (!titleText.empty())
        builder.PopContext();
}

void WindowScreen::AddFooter(graph::GraphBuilder& builder, const agui::Widget* window)
{
    const agui::Widget* footer = agui::dialogButtons(window);
    if (!footer || !Shows(footer))
        return;
    builder.BeginStop("buttons");
    AddSubtree(builder, "buttons", footer);
}

bool WindowScreen::TypingIn(const graph::GraphNode& node)
{
    // The retained render may be frames old, so the widget is compared before it is read: when it
    // is the game's focused widget, it is alive.
    // A node may type into a field beside it (a slider's value field), so a focused sibling counts
    // too; only the focused widget and its parent, both alive, are read.
    auto* widget = static_cast<const agui::Widget*>(node.Vtable.HostTag);
    const agui::Widget* focused = s_gui ? agui::focusedWidget(s_gui) : nullptr;
    if (!widget || !focused || agui::kind(focused) != agui::Kind::TextBox || agui::readOnly(focused))
        return false;
    if (widget == focused)
        return true;
    const agui::Widget* row = agui::parent(focused);
    return row && std::ranges::find(agui::children(row), widget) != agui::children(row).end();
}

void WindowScreen::OnCursorMoved(const graph::GraphNode& node)
{
    auto* widget = static_cast<const agui::Widget*>(node.Vtable.HostTag);
    if (!widget)
        return;
    agui::scrollIntoView(widget);
    if (agui::isFocusable(widget))
        agui::focus(widget);
}

void WindowScreen::OnPop() { _window = nullptr; }

} // namespace fa::screens
