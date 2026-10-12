#pragma once

// The Mods window (ModsGui). Stops, after the tabs (Manage, Browse and install, Updates):
// On the Manage tab:
//   mods      — the installed mods, read without touching the game. A row each: the name ("Space Age
//               2.1.21"), where Enter clicks the mod, selecting it so the info shows it; then its
//               checkbox, where Enter enables or disables it. The all-mods checkbox comes first.
//   info      — the selected mod's details (see below)
//   search    — the search field and the column sort buttons
// On the Browse and install tab, the mod portal:
//   search    — the sort order, the search field and the search's status
//   results   — a line per mod found ("Rate Calculator 3.4.1, by raiguard, Utilities, ..."), its
//               summary beside it, Enter selecting it; then how many were found and the page buttons
//   info      — the selected mod's details
//   filters   — the filters under their headings: a filter's checkbox with the mods it matches, then
//               the button excluding it
// On the Updates tab:
//   mods      — the mods with a newer version, laid out as on Manage, the checkbox ticking it for
//               the update; then the game's status (all up to date, not logged in)
//   info      — the selected mod's details
//   search    — the search field, Update selected, refresh, and the column sort buttons
// Then on every tab:
//   downloads — while mods download, their progress and Cancel
//   buttons   — Back and Confirm
//
// A mod's details: its name, then bookmark, install, changelog and delete; the description; the
// facts a line each ("Version 2.5.0"); the dependencies and incompatibilities, each with the status
// the game gives it, Enter showing that mod.

#include <chrono>
#include <string>

#include "WindowScreen.hpp"

namespace fa::screens
{

class ModsScreen final : public WindowScreen
{
public:
    bool Handles(const agui::Widget* window) const override;
    const char* TakeSuggestedLanding() override;
    void OnPop() override;

protected:
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;

private:
    void BuildManage(graph::GraphBuilder& builder, const agui::Widget* window);
    void BuildExplore(graph::GraphBuilder& builder, const agui::Widget* window);
    void BuildUpdates(graph::GraphBuilder& builder, const agui::Widget* window);
    // A mod's details, as the stop "info".
    void AddInfo(graph::GraphBuilder& builder, const agui::Widget* pane);

    std::string _shownMod; // the mod whose details the last build declared
    // A dependency pressed: focus lands on the details once they show another mod than this.
    bool _following = false;
    std::string _followedFrom;
    std::chrono::steady_clock::time_point _followedAt;
    bool _landOnMod = false;
    // The Browse tab's count of mods found, said as it changes while the tab stays shown.
    std::string _found;
    bool _browsing = false;    // the last build showed the Browse tab
    bool _browsingNow = false; // this one does
};

} // namespace fa::screens
