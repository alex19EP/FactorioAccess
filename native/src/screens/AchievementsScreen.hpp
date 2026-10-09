#pragma once

// The achievements window (AchievementGui), opened from the side menu in place of the inventory.
// The game has no key of its own for it.
//
// Stops, read in the window title's context:
//   - summary: "Earned 12 of 47" with the bar's percentage, then the "modded game" and "played too
//     little" notes when the game shows them;
//   - search: the title bar's search button, and its field while open (opening it lands there);
//   - achievements: a card each, in the game's order (earned, then normal, then failed; the hidden
//     ones only once earned), as many as the search leaves.
//
// A card reads as AchievementCards.hpp says: its state, then its name, description and progress
// ("12.3k/1.0M", "Remaining time: 2:41:10"); Y reads why a failed one failed, which the game shows
// only as a tooltip. Enter on a normal card presses its track button. The game refreshes a card's
// progress only while the card is scrolled into view, which the cursor does.

#include "EntityWindowScreen.hpp"

namespace fa::screens
{

class AchievementsScreen final : public EntityWindowScreen
{
public:
    const char* TakeSuggestedLanding() override;
    void OnPop() override;

protected:
    bool Handles(const agui::Widget* window) const override;
    void BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window) override;

private:
    bool _searching = false;        // the search field showed at the last build
    const char* _landing = nullptr; // where the next render lands, once
};

} // namespace fa::screens
