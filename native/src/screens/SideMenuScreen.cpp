#include "SideMenuScreen.hpp"

#include <string>

#include "AguiNodes.hpp"
#include "parts.h"
#include "vocab.h"

namespace fa::screens
{

namespace
{

using agui::Kind;
using agui::Widget;

// The side menu, while it is on screen over a loaded game with no menu in front of it.
const Widget* FindMenu()
{
    if (!agui::inGame() || agui::menuStateWindow())
        return nullptr;
    const Widget* menu = agui::sideMenu();
    return menu && agui::visible(menu) ? menu : nullptr;
}

} // namespace

std::string SideMenuScreen::Name() const { return vocab::kSideMenu; }

bool SideMenuScreen::IsActive()
{
    if (parts::current() != parts::Part::SideMenu)
        return false;
    _menu = FindMenu();
    // Nothing to go back to once the game is gone.
    if (!agui::inGame())
        parts::close();
    return _menu != nullptr;
}

void SideMenuScreen::Build(graph::GraphBuilder& builder)
{
    if (!_menu || FindMenu() != _menu)
        return;
    const Widget* table = FindDescendant(_menu, "agui::Table");
    if (!table)
        return;
    const Widget* mute = agui::sideMenuMuteButton(_menu);
    builder.BeginStop("buttons");
    AddGrid(
        builder, "buttons", table,
        [](const Widget* cell) { return agui::visible(cell) && agui::kind(cell) == Kind::Button; },
        [this, mute](const Widget* cell)
        {
            graph::NodeVtable vtable = ControlNode(cell);
            if (cell == mute)
                return vtable;
            // The window it opens takes over from the side menu.
            vtable.OnActivate = [this, cell]()
            {
                if (!agui::enabled(cell))
                    return;
                agui::press(cell, agui::MouseButton::Left, false, false);
                _opening = true;
                parts::close();
            };
            return vtable;
        });
}

void SideMenuScreen::OnEscape() { parts::close(); }

void SideMenuScreen::OnPush() { _opening = false; }

void SideMenuScreen::OnPop() { _menu = nullptr; }

std::string SideMenuScreen::LeaveLine() const
{
    // Left with Ctrl+Tab or Escape, not covered by a menu and not for a window it opened: the side
    // menu is no longer in use.
    return parts::current() == parts::Part::None && !_opening ? std::string(vocab::kMap) : std::string();
}

} // namespace fa::screens
