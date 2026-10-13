#include "TrainScreen.hpp"

#include "ScheduleNodes.hpp"
#include "game.h"

namespace fa::screens
{

using agui::Widget;

bool TrainScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "TrainGui"); }

void TrainScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    agui::TrainWindowParts parts = agui::trainWindowParts(window);
    AddTitledWindow(builder, "train", window, {parts.tabs, parts.schedule});
    const bool tabs = Shows(parts.tabs);
    if (tabs)
        if (const Widget* header = FindDescendant(parts.tabs, "agui::Table"))
            AddSubtree(builder, "train/tabs", header);

    if (Shows(parts.schedule))
    {
        AddSchedule(builder, "schedule", parts.schedule,
            [this](std::string key, std::function<bool()> ready) { LandWhen(std::move(key), std::move(ready)); });
        return;
    }
    if (tabs)
    {
        builder.BeginStop("cargo");
        AddSubtree(builder, "cargo", agui::member(parts.tabs, game::layout.tabbedPaneContent));
    }
}

} // namespace fa::screens
