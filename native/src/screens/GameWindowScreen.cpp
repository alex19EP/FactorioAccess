#include "GameWindowScreen.hpp"

#include <algorithm>

namespace fa::screens
{

bool GameWindowScreen::Handles(const agui::Widget* window) const
{
    // The character screen, Factoriopedia, the technology window and the alerts window have screens
    // of their own.
    if (!agui::derivesFrom(window, "GameGui") || agui::derivesFrom(window, "ControllerGui")
        || window == agui::factoriopedia().window
        || window == agui::technologyWindow().window || window == agui::alertsWindow().window)
        return false;
    return std::ranges::none_of(
        _recipes, [window](const EntityWindowScreen* recipe) { return recipe->Handles(window); });
}

void GameWindowScreen::BuildWindow(graph::GraphBuilder& builder, const agui::Widget* window)
{
    AddTitledWindow(builder, "window", window);
}

} // namespace fa::screens
