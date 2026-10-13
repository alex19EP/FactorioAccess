#include "TrainScreen.hpp"

#include "ScheduleNodes.hpp"
#include "game.h"
#include "parts.h"
#include "vocab.h"

namespace fa::screens
{

using agui::Widget;

bool TrainScreen::IsActive()
{
    const bool open = FindWindow().window != nullptr;
    parts::setAvailable(parts::Part::Map, open);
    if (parts::current() == parts::Part::Map)
    {
        if (open)
            return false;
        parts::close();
    }
    return EntityWindowScreen::IsActive();
}

std::string TrainScreen::LeaveLine() const
{
    return parts::current() == parts::Part::Map ? std::string(vocab::kMap) : std::string();
}

bool TrainScreen::Handles(const Widget* window) const { return agui::derivesFrom(window, "TrainGui"); }

void TrainScreen::BuildWindow(graph::GraphBuilder& builder, const Widget* window)
{
    agui::TrainWindowParts parts = agui::trainWindowParts(window);
    AddTitledWindow(builder, "train", window, {parts.tabs, parts.schedule});
    const bool tabs = Shows(parts.tabs);
    if (tabs)
        if (const Widget* header = FindDescendant(parts.tabs, "agui::Table"))
            AddSubtree(builder, "train/tabs", header);

    // The schedule keeps its visible flag while the other tab shows: the selected tab tells.
    if (Shows(parts.schedule) && (!tabs || agui::tabSelected(parts.scheduleTab)))
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
