#pragma once

// The Mods window (ModsGui). Stops:
//   tabs    — Manage, Browse and install, Updates
//   mods    — on the Manage tab, the installed mods, read without touching the game. A row each:
//             the name ("Space Age 2.1.21"), where Enter clicks the mod, selecting it so the info
//             shows it; then its checkbox, where Enter enables or disables it. The all-mods
//             checkbox comes first.
//   info    — the selected mod's info: name, changelog and delete, description, version, author,
//             dependencies
//   content — on the other tabs, the tab's page read generically
//   buttons — Back and Confirm
//   search  — the search field and the column sort buttons

#include "WindowScreen.hpp"

namespace fa::screens
{

class ModsScreen final : public WindowScreen
{
public:
    bool Handles(const agui::Widget* window) const override;

protected:
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;

private:
    void BuildManage(graph::GraphBuilder& builder, const agui::Widget* window);
};

} // namespace fa::screens
