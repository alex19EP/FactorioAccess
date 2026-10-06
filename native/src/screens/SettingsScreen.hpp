#pragma once

// The settings windows that open from Settings (SettingsGui): Graphics, Sound, Interface, Other,
// The rest and Mod settings. Controls has a recipe of its own. Stops:
//   settings — the window's content, read generically: a labelled control per line, sections
//              as contexts
//   buttons  — the reset button and the footer (Back, Confirm)
//   search   — the search field, where the window has one

#include "WindowScreen.hpp"

namespace fa::screens
{

class SettingsScreen final : public WindowScreen
{
public:
    bool Handles(const agui::Widget* window) const override;

protected:
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;
};

/// The search stop the settings windows share: the window's search bar and its popup field.
void AddSearchStop(graph::GraphBuilder& builder, const agui::Widget* window);

/// The settings windows' buttons stop: the reset button, then the dialog footer.
void AddSettingsButtons(graph::GraphBuilder& builder, const agui::Widget* window);

} // namespace fa::screens
