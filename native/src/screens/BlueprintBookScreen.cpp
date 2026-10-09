#include "BlueprintBookScreen.hpp"

#include "BlueprintLists.hpp"
#include "game.h"

namespace fa::screens
{

using agui::Widget;
using game::layout;

bool BlueprintBookScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "BlueprintBookGui"); }

void BlueprintBookScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    AddBook(builder,
        {agui::member(window, layout.bookGuiName), agui::member(window, layout.bookGuiRename),
            agui::member(window, layout.bookGuiDescription), agui::member(window, layout.bookGuiNavigation)});
    // The header's subheader: the book's item name as its caption, then its buttons.
    AddSubheaderButtons(builder, "buttons", agui::member(window, layout.bookGuiHeader));
    builder.BeginStop("contents");
    AddBlueprintList(builder, "contents", agui::member(window, layout.bookGuiList));
    AddListView(builder, agui::member(window, layout.bookGuiInside));
    AddInventory(builder, agui::entityWindowParts(window));
}

} // namespace fa::screens
