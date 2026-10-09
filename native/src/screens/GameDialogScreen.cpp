#include "GameDialogScreen.hpp"

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "AguiNodes.hpp"

namespace fa::screens
{

using agui::Widget;

bool GameDialogScreen::Handles(const Widget* window) const
{
    // Windows that float too, with recipes of their own.
    if (agui::derivesFromTemplate(window, "SelectListGui") || agui::derivesFrom(window, "BlueprintSetupGui")
        || agui::derivesFrom(window, "TipsAndTricksGui")
        || window == agui::factoriopedia().window
        || window == agui::technologyWindow().window || window == agui::alertsWindow().window)
        return false;
    return agui::dialogButtons(window) || agui::derivesFrom(window, "FloatingGuiWindow");
}

void GameDialogScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* title = agui::frameTitle(window);
    const Widget* footer = agui::dialogButtons(window);
    std::vector<const Widget*> skip{title, footer};
    std::ranges::copy(FindAll(window, "CloseButton"), std::back_inserter(skip));
    std::string titleText = title && Shows(title) ? LabelText(title) : std::string();

    builder.BeginStop("content");
    if (!titleText.empty())
        builder.PushContext(titleText);
    AddSubtree(builder, "content", window, std::move(skip));
    if (!titleText.empty())
        builder.PopContext();

    if (footer && Shows(footer) && HasContent(footer))
    {
        builder.BeginStop("buttons");
        AddSubtree(builder, "buttons", footer);
    }
}

} // namespace fa::screens
