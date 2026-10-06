#pragma once

// Mod settings (ModSettingsGui), from Settings or the map generator. Stops:
//   tabs     — Startup, Map, Per player
//   settings — the selected tab's page: a row per setting under its mod's name, the control named
//              by the setting, then the setting's reset button while it is off its default
//   buttons  — the reset-all button and the footer (Back, Confirm)
//   search   — the search field

#include "WindowScreen.hpp"

namespace fa::screens
{

class ModSettingsScreen final : public WindowScreen
{
public:
    bool Handles(const agui::Widget* window) const override;

protected:
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

} // namespace fa::screens
