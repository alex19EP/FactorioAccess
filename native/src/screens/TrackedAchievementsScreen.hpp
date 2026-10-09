#pragma once

// The achievements the player tracks at the top left (GameView::trackedAchievementHolder), the part
// of the HUD after the crafting queue on Ctrl+Tab (see parts.h).
//
// One stop: a card each, as AchievementCards.hpp reads them: the achievement's icon by its name and
// its progress, earned or failed first when it is. Y reads the card's tooltip, Enter presses its
// button, which stops tracking it. With nothing tracked the stop says so.
//
// The game has no window to close here, so Escape is ours: it goes back to what is open.

#include <string>

#include "agui.h"
#include "navigator/Screen.hpp"

namespace fa::screens
{

class TrackedAchievementsScreen final : public nav::Screen
{
public:
    std::string Name() const override;
    const char* DiagName() const override { return "tracked achievements"; }
    bool RemembersCursor() const override { return true; }
    // Over the window it was opened from, as the quickbar is.
    int Layer() const override { return 1; }
    bool IsActive() override;
    void Build(graph::GraphBuilder& builder) override;
    bool ClaimsEscape() const override { return true; }
    void OnEscape() override;
    void OnPop() override;
    std::string LeaveLine() const override;

private:
    const agui::Widget* _holder = nullptr;
};

} // namespace fa::screens
