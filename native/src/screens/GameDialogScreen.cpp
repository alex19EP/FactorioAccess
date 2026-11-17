#include "GameDialogScreen.hpp"

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "AguiNodes.hpp"

namespace fa::screens
{

using agui::Widget;

namespace
{

constexpr const char* kSearchField = "content/search/field";

} // namespace

bool GameDialogScreen::Handles(const Widget* window) const
{
    // Windows that float too, with recipes of their own.
    if (agui::derivesFromTemplate(window, "SelectListGui") || agui::derivesFrom(window, "BlueprintSetupGui")
        || agui::derivesFrom(window, "TipsAndTricksGui") || agui::derivesFrom(window, "EditInterruptGui")
        || window == agui::factoriopedia().window || window == agui::technologyWindow().window
        || window == agui::alertsWindow().window)
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

    // A search in the title bar (the planet list a platform's character drops to) opens its field
    // under it: the button and the field come first, and opening the search lands on the field.
    const Widget* search = FindDescendant(window, "SearchBar");
    const Widget* popup = FindDescendant(window, "SearchPopup");
    const Widget* field = popup && Shows(popup) ? FindDescendant(popup, "agui::TextField") : nullptr;
    skip.push_back(search);
    skip.push_back(popup);

    builder.BeginStop("content");
    if (!titleText.empty())
        builder.PushContext(titleText);
    if (search && Shows(search))
    {
        builder.StartRow("content/search");
        builder.AddItem(graph::ControlId::Referenced(search, "content/search/button"), ControlNode(search));
        if (field)
            builder.AddItem(graph::ControlId::Referenced(field, kSearchField), ControlNode(field));
        builder.EndRow();
    }
    if (field && !_searching)
        LandWhen(kSearchField, []() { return true; });
    _searching = field != nullptr;
    AddSubtree(builder, "content", window, std::move(skip));
    if (!titleText.empty())
        builder.PopContext();

    if (footer && Shows(footer) && HasContent(footer))
    {
        builder.BeginStop("buttons");
        AddSubtree(builder, "buttons", footer);
    }
}

void GameDialogScreen::OnPop()
{
    EntityWindowScreen::OnPop();
    _searching = false;
}

} // namespace fa::screens
