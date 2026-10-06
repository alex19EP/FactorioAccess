#pragma once

// The button menus (MenuGui): the main menu, Single player, Multiplayer, the map editor menus and
// the game menu. Stops:
//   menu    — the buttons top to bottom: Continue (on the main menu), the menu's own, Exit or Back
//   panel/n — on the main menu, each panel beside it: the language selector, the background
//             simulation selector, the Space Age advert
//   version — on the main menu, the game version shown in its corner

#include "WindowScreen.hpp"

namespace fa::screens
{

class MenuScreen final : public WindowScreen
{
public:
    bool Handles(const agui::Widget* window) const override;

protected:
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
