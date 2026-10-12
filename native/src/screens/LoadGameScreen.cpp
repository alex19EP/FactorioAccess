#include "LoadGameScreen.hpp"

#include "AguiNodes.hpp"
#include "SaveLists.hpp"
#include "game.h"

namespace fa::screens
{

using agui::Widget;
using game::layout;

bool LoadGameScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "LoadMapGui"); }

void LoadGameScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    const Widget* info = agui::member(window, layout.loadMapInfo);
    AddSaves(builder, window, agui::member(window, layout.loadMapList), agui::member(info, layout.mapInfoDelete));

    builder.BeginStop("details");
    AddSubtree(builder, "details", info);

    AddFooter(builder, window);

    AddSaveSearch(builder, window);
}

} // namespace fa::screens
